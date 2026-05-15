#include "odrive_uart.h"
#include "peripheral_ctrl.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"

/* LCD 相关 */
#include "lcd.h"
#include "lcd_init.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ODRIVE";

/* ============================================================
 *  内部状态
 * ============================================================ */
static odrive_status_t  s_status;
static SemaphoreHandle_t s_status_mutex = NULL;
static odrive_ble_cb_t   s_ble_cb       = NULL;

/* ============================================================
 *  帧解析工具
 * ============================================================ */

/**
 * @brief 解析 $ST,... 主动上报帧
 *        帧格式: $ST,<vb>,<f>,<M0h>,<M0c>,<M0v>,<M0i>,<M0e>,<M0s>,
 *                     <M1h>,<M1c>,<M1v>,<M1i>,<M1e>,<M1s>;
 * @return true  解析成功
 */
static bool parse_st_frame(const char *line, odrive_status_t *out)
{
    /* 快速前缀检查 */
    if (strncmp(line, "$ST,", 4) != 0) return false;

    /* 寻找结尾 ';' */
    const char *end = strchr(line + 4, ';');
    if (!end) return false;

    /* 复制字段区域到临时缓冲区，避免修改原始数据 */
    char buf[256];
    size_t field_len = (size_t)(end - (line + 4));
    if (field_len >= sizeof(buf)) return false;
    memcpy(buf, line + 4, field_len);
    buf[field_len] = '\0';

    /* 按逗号分割，共 14 个字段 */
    char *fields[14];
    int n = 0;
    char *tok = strtok(buf, ",");
    while (tok && n < 14) {
        fields[n++] = tok;
        tok = strtok(NULL, ",");
    }
    if (n < 14) return false;

    out->vbus_v    = strtof(fields[0],  NULL);
    out->drv_fault = (int)strtol(fields[1], NULL, 10);
    out->m0_hall   = (uint8_t)strtoul(fields[2], NULL, 10);
    out->m0_count  = (int)strtol(fields[3], NULL, 10);
    out->m0_vel    = strtof(fields[4],  NULL);
    out->m0_iq     = strtof(fields[5],  NULL);
    out->m0_error  = (int)strtol(fields[6], NULL, 10);
    out->m0_status = (uint8_t)strtoul(fields[7], NULL, 10);
    out->m1_hall   = (uint8_t)strtoul(fields[8], NULL, 10);
    out->m1_count  = (int)strtol(fields[9], NULL, 10);
    out->m1_vel    = strtof(fields[10], NULL);
    out->m1_iq     = strtof(fields[11], NULL);
    out->m1_error  = (int)strtol(fields[12], NULL, 10);
    out->m1_status = (uint8_t)strtoul(fields[13], NULL, 10);
    return true;
}

/* ============================================================
 *  LED 状态指示辅助
 *   LED1: ODrive 通信正常 (收到$ST帧则常亮；超时1s则熄灭)
 *   LED2: M0 使能中 (m0_status bit1=1)
 *   LED3: M1 使能中 (m1_status bit1=1)
 *   LED4: 有错误时快闪 (由外部BLE状态控制，此处不处理)
 * ============================================================ */
static void update_status_leds(const odrive_status_t *s)
{
    led_set(1, true);                           /* 收到帧 -> LED1 亮 */
    led_set(2, (s->m0_status & 0x02) != 0);    /* M0 使能 */
    led_set(3, (s->m1_status & 0x02) != 0);    /* M1 使能 */
    /* LED4 保留给 BLE 连接状态，在 ble_uart.c 中控制 */
}

/* ============================================================
 *  后台接收任务
 * ============================================================ */
static void odrive_rx_task(void *arg)
{
    static uint8_t  raw[ODRIVE_RX_BUF_SIZE];
    static char     line_buf[512];
    static int      line_pos = 0;
    static uint32_t last_rx_tick = 0;

    ESP_LOGI(TAG, "ODrive RX task started (UART%d TX=IO%d RX=IO%d)",
             ODRIVE_UART_NUM, ODRIVE_TX_PIN, ODRIVE_RX_PIN);

    while (1) {
        int len = uart_read_bytes(ODRIVE_UART_NUM, raw,
                                  sizeof(raw) - 1,
                                  pdMS_TO_TICKS(100));

        /* 超时 1s 无数据则熄灭 LED1 */
        uint32_t now_tick = (uint32_t)(esp_timer_get_time() / 1000);
        if (len <= 0) {
            if ((now_tick - last_rx_tick) > 1000) {
                led_set(1, false);
            }
            continue;
        }
        last_rx_tick = now_tick;

        /* 逐字节拼行，以 '\n' 为行结束符 */
        for (int i = 0; i < len; i++) {
            char c = (char)raw[i];

            if (c == '\n' || c == '\r') {
                if (line_pos > 0) {
                    line_buf[line_pos] = '\0';

                    /* ---- BLE 转发（整行原始文本） ---- */
                    if (s_ble_cb) {
                        char fwd[520];
                        int fwd_len = snprintf(fwd, sizeof(fwd), "%s\n", line_buf);
                        if (fwd_len > 0) {
                            s_ble_cb((const uint8_t *)fwd, (size_t)fwd_len);
                        }
                    }

                    /* ---- 解析 $ST 帧 ---- */
                    if (strncmp(line_buf, "$ST,", 4) == 0) {
                        odrive_status_t tmp = {0};
                        if (parse_st_frame(line_buf, &tmp)) {
                            tmp.timestamp_ms = now_tick;
                            tmp.valid        = true;

                            xSemaphoreTake(s_status_mutex, portMAX_DELAY);
                            s_status = tmp;
                            xSemaphoreGive(s_status_mutex);

                            update_status_leds(&tmp);

                            ESP_LOGD(TAG,
                                     "ST: VB=%.1fV M0v=%.1f M0i=%.2f M1v=%.1f M1i=%.2f",
                                     tmp.vbus_v, tmp.m0_vel, tmp.m0_iq,
                                     tmp.m1_vel, tmp.m1_iq);
                        }
                    }

                    line_pos = 0;
                }
                continue;
            }

            /* 防越界 */
            if (line_pos < (int)(sizeof(line_buf) - 1)) {
                line_buf[line_pos++] = c;
            } else {
                /* 行缓冲区满，丢弃当前行 */
                ESP_LOGW(TAG, "Line buffer overflow, discarding");
                line_pos = 0;
            }
        }
    }
}

/* ============================================================
 *  公共接口
 * ============================================================ */
esp_err_t odrive_uart_init(odrive_ble_cb_t ble_cb)
{
    s_ble_cb = ble_cb;
    memset(&s_status, 0, sizeof(s_status));

    s_status_mutex = xSemaphoreCreateMutex();
    if (!s_status_mutex) {
        ESP_LOGE(TAG, "Failed to create status mutex");
        return ESP_ERR_NO_MEM;
    }

    /* 安装 UART 驱动 */
    const uart_config_t uart_cfg = {
        .baud_rate  = ODRIVE_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_driver_install(ODRIVE_UART_NUM,
                                        ODRIVE_RX_BUF_SIZE * 2,
                                        ODRIVE_RX_BUF_SIZE,
                                        0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %d", ret);
        return ret;
    }

    ret = uart_param_config(ODRIVE_UART_NUM, &uart_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %d", ret);
        return ret;
    }

    ret = uart_set_pin(ODRIVE_UART_NUM,
                       ODRIVE_TX_PIN, ODRIVE_RX_PIN,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %d", ret);
        return ret;
    }

    /* 启动后台接收任务 */
    BaseType_t ok = xTaskCreate(odrive_rx_task, "odrive_rx",
                                4096, NULL, 5, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create odrive_rx task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "ODrive UART initialized: IO%d(TX) IO%d(RX) %d baud",
             ODRIVE_TX_PIN, ODRIVE_RX_PIN, ODRIVE_BAUD_RATE);
    return ESP_OK;
}

void odrive_uart_get_status(odrive_status_t *out)
{
    if (!out || !s_status_mutex) return;
    xSemaphoreTake(s_status_mutex, portMAX_DELAY);
    *out = s_status;
    xSemaphoreGive(s_status_mutex);
}

int odrive_uart_send_cmd(const char *cmd)
{
    if (!cmd) return -1;
    int len = (int)strlen(cmd);
    return uart_write_bytes(ODRIVE_UART_NUM, cmd, len);
}

void odrive_uart_set_ble_cb(odrive_ble_cb_t cb)
{
    s_ble_cb = cb;
}

/* ============================================================
 *  LCD 状态栏刷新
 *  在屏幕底部 32 像素区域显示两行 ODrive 状态
 *  (建议约 2Hz 调用，避免刷新频率过高影响图片渲染)
 * ============================================================ */
void odrive_uart_lcd_update(void)
{
    odrive_status_t s;
    odrive_uart_get_status(&s);

    /* 状态栏区域：y = LCD_H-32 ~ LCD_H-1 */
    const uint16_t y0 = LCD_H - 32;
    const uint16_t y1 = LCD_H - 1;

    /* 清底部背景 */
    LCD_Fill(0, y0, LCD_W - 1, y1, BLACK);

    if (!s.valid) {
        LCD_ShowString(2, y0,
                       (const uint8_t *)"ODrive: No data",
                       RED, BLACK, 16, 0);
        return;
    }

    /* 第一行：母线电压 + M0 速度/电流/状态 */
    char line1[64];
    snprintf(line1, sizeof(line1),
             "VB:%.1fV M0 v=%.1f i=%.2fA s=%d e=%d",
             s.vbus_v, s.m0_vel, s.m0_iq, s.m0_status, s.m0_error);

    /* 第二行：M1 速度/电流/状态 + DRV 故障 */
    char line2[64];
    snprintf(line2, sizeof(line2),
             "DRV:%d  M1 v=%.1f i=%.2fA s=%d e=%d",
             s.drv_fault, s.m1_vel, s.m1_iq, s.m1_status, s.m1_error);

    LCD_ShowString(2, y0,      (const uint8_t *)line1, WHITE, BLACK, 16, 0);
    LCD_ShowString(2, y0 + 16, (const uint8_t *)line2, CYAN,  BLACK, 16, 0);
}
