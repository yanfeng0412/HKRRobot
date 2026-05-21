#include "ble_uart.h"
#include "peripheral_ctrl.h"

/* NimBLE */
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include <assert.h>

static const char *TAG = "BLE_UART";

/* ============================================================
 *  NUS 128-bit UUIDs (小端序字节数组)
 *  原始: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 * ============================================================ */
#define NUS_SVC_UUID128  \
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, \
    0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e

/* RX: 6E400002 ... */
#define NUS_RX_UUID128   \
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, \
    0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e

/* TX: 6E400003 ... */
#define NUS_TX_UUID128   \
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, \
    0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e

/* ============================================================
 *  内部状态
 * ============================================================ */
static uint16_t          s_conn_handle  = BLE_HS_CONN_HANDLE_NONE;
static uint16_t          s_tx_val_hdl   = 0;
static ble_uart_rx_cb_t  s_rx_cb        = NULL;
static bool              s_notify_enable = false;
/* MTU 协商后的最大负载字节数 (= MTU - 3, 默认 20) */
static uint16_t          s_max_payload  = 20;

static const char *DEVICE_NAME = "HKR_Robot";

/* ============================================================
 *  GATT: RX Characteristic 访问回调
 *  手机写入数据 -> 回调 -> 转发给 ODrive
 * ============================================================ */
static int nus_rx_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;

    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len == 0 || len > 512) return 0;

    uint8_t buf[512];
    uint16_t copied = 0;
    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &copied);
    if (rc != 0) return rc;

    ESP_LOGD(TAG, "BLE RX %d bytes", copied);

    if (s_rx_cb) {
        s_rx_cb(buf, (size_t)copied);
    }
    return 0;
}

/* ============================================================
 *  GATT: TX Characteristic 访问回调 (占位，客户端只订阅 notify)
 * ============================================================ */
static int nus_tx_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    return 0;
}

/* ============================================================
 *  GATT 服务表
 * ============================================================ */
static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(NUS_SVC_UUID128),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                /* RX: 手机 -> ESP32 (Write / Write No Response) */
                .uuid       = BLE_UUID128_DECLARE(NUS_RX_UUID128),
                .access_cb  = nus_rx_access_cb,
                .flags      = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                /* TX: ESP32 -> 手机 (Notify) */
                .uuid       = BLE_UUID128_DECLARE(NUS_TX_UUID128),
                .access_cb  = nus_tx_access_cb,
                .val_handle = &s_tx_val_hdl,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            { 0 }, /* 结束标记 */
        },
    },
    { 0 }, /* 结束标记 */
};

/* ============================================================
 *  广播参数
 * ============================================================ */
static void ble_uart_advertise(void);

static void ble_uart_on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE reset, reason=%d", reason);
    s_conn_handle   = BLE_HS_CONN_HANDLE_NONE;
    s_notify_enable = false;
    s_max_payload   = 20;   /* 重置为默认分片大小 */
    led_set(4, false);
}

static void ble_uart_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    ble_uart_advertise();
}

static int ble_uart_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "BLE connected, handle=%d", s_conn_handle);
            led_set(4, true);           /* LED4 亮表示 BLE 已连接 */

            /* 请求 MTU 协商（最高 247 字节负载，减少分片次数）
             * 仅设置首选 MTU，对端（手机）会在连接后发起 MTU 交换请求，
             * 届时 BLE_GAP_EVENT_MTU 回调会更新 s_max_payload。 */
            ble_att_set_preferred_mtu(250);

            /* 请求更短连接间隔，提升 UART 透传实时性
             *  itvl: 单位 1.25ms，8 = 10ms，16 = 20ms
             *  supervision_timeout: 单位 10ms，400 = 4s */
            struct ble_gap_upd_params upd = {
                .itvl_min            = 8,
                .itvl_max            = 16,
                .latency             = 0,
                .supervision_timeout = 400,
                .min_ce_len          = BLE_GAP_INITIAL_CONN_MIN_CE_LEN,
                .max_ce_len          = BLE_GAP_INITIAL_CONN_MAX_CE_LEN,
            };
            ble_gap_update_params(s_conn_handle, &upd);
        } else {
            ESP_LOGW(TAG, "BLE connect failed, status=%d", event->connect.status);
            s_conn_handle   = BLE_HS_CONN_HANDLE_NONE;
            s_notify_enable = false;
            led_set(4, false);
            ble_uart_advertise();       /* 重新广播 */
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE disconnected, reason=%d", event->disconnect.reason);
        s_conn_handle   = BLE_HS_CONN_HANDLE_NONE;
        s_notify_enable = false;
        s_max_payload   = 20;   /* 断连后重置 */
        led_set(4, false);
        ble_uart_advertise();
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == s_tx_val_hdl) {
            s_notify_enable = (event->subscribe.cur_notify != 0);
            ESP_LOGI(TAG, "NUS TX notify %s",
                     s_notify_enable ? "enabled" : "disabled");
        }
        break;

    case BLE_GAP_EVENT_MTU:
        /* MTU 协商完成，更新分片大小（payload = MTU - 3） */
        if (event->mtu.value > 3) {
            s_max_payload = event->mtu.value - 3;
        }
        ESP_LOGI(TAG, "MTU update: conn=%d mtu=%d payload=%d",
                 event->mtu.conn_handle, event->mtu.value, s_max_payload);
        break;

    default:
        break;
    }
    return 0;
}

static void ble_uart_advertise(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags                = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl            = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.name                  = (uint8_t *)DEVICE_NAME;
    fields.name_len              = (uint8_t)strlen(DEVICE_NAME);
    fields.name_is_complete      = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields failed: %d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_uart_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed: %d", rc);
    } else {
        ESP_LOGI(TAG, "BLE advertising as '%s'", DEVICE_NAME);
    }
}

/* NimBLE 主机任务 */
static void nimble_host_task(void *param)
{
    nimble_port_run();          /* 阻塞，直到 nimble_port_stop() 被调用 */
    nimble_port_freertos_deinit();
}

/* ============================================================
 *  公共接口
 * ============================================================ */
esp_err_t ble_uart_init(ble_uart_rx_cb_t rx_cb)
{
    s_rx_cb = rx_cb;

    /* 初始化 NimBLE */
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", ret);
        return ret;
    }

    /* 注册 GAP/GATT 基础服务 */
    ble_svc_gap_init();
    ble_svc_gatt_init();

    /* 注册 NUS 服务 */
    int rc = ble_gatts_count_cfg(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return ESP_FAIL;
    }
    rc = ble_gatts_add_svcs(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return ESP_FAIL;
    }

    /* 设置设备名称 */
    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    assert(rc == 0);

    /* 注册同步/重置回调 */
    ble_hs_cfg.reset_cb  = ble_uart_on_reset;
    ble_hs_cfg.sync_cb   = ble_uart_on_sync;
    ble_hs_cfg.gatts_register_cb = NULL;
    ble_hs_cfg.store_status_cb   = ble_store_util_status_rr;

    /* 启动 NimBLE 主机任务 */
    nimble_port_freertos_init(nimble_host_task);

    ESP_LOGI(TAG, "BLE UART (NUS) initialized, device name: %s", DEVICE_NAME);
    return ESP_OK;
}

esp_err_t ble_uart_send(const uint8_t *data, size_t len)
{
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE) return ESP_ERR_INVALID_STATE;
    if (!s_notify_enable) return ESP_ERR_INVALID_STATE;
    if (s_tx_val_hdl == 0) return ESP_ERR_INVALID_STATE;

    size_t offset = 0;
    while (offset < len) {
        /* 每片发前再次检查连接状态 */
        if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE || !s_notify_enable) {
            return ESP_ERR_INVALID_STATE;
        }

        uint16_t chunk = (uint16_t)((len - offset) > s_max_payload
                                     ? s_max_payload : (len - offset));

        /* 拥塑重试: NimBLE 队列满时等待一个连接间隔再重试 */
        int rc = BLE_HS_ENOMEM;
        for (int retry = 0; retry < 5; retry++) {
            struct os_mbuf *om = ble_hs_mbuf_from_flat(data + offset,
                                                        (uint16_t)chunk);
            if (om == NULL) {
                /* mbuf 池耗尽，等待一个连接间隔 (~15ms) */
                vTaskDelay(pdMS_TO_TICKS(15));
                continue;
            }
            /* om 的所有权常无条件转入 ble_gatts_notify_custom */
            rc = ble_gatts_notify_custom(s_conn_handle, s_tx_val_hdl, om);
            if (rc == 0) {
                break;  /* 发送成功 */
            }
            if (rc == BLE_HS_EBUSY || rc == BLE_HS_ENOMEM) {
                /* BLE 层拥塑，等待一个连接间隔 (~15ms) 后重试 */
                vTaskDelay(pdMS_TO_TICKS(15));
            } else {
                /* 不可恢复错误（如已断连），丢弃剩余数据 */
                ESP_LOGW(TAG, "notify err=%d, dropping %u bytes",
                         rc, (unsigned)(len - offset));
                return ESP_FAIL;
            }
        }

        if (rc != 0) {
            /* 重试 5 次仍失败，丢弃剩余数据避免无限阅塞 */
            ESP_LOGW(TAG, "notify timeout, dropping %u bytes",
                     (unsigned)(len - offset));
            break;
        }
        offset += chunk;
    }
    return ESP_OK;
}

bool ble_uart_is_connected(void)
{
    return (s_conn_handle != BLE_HS_CONN_HANDLE_NONE);
}
