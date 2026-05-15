#include <stdio.h>
#include <string.h>
#include "lcd_demo.h"
#include "lcd.h"
#include "lcd_init.h"
#include "cst816.h"
#include "jpeg_decoder.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/uart_vfs.h"
#include "dify_chat.h"
#include "tts_sst_demo.h"
#include "servo_control.h"

/* ---- 新增模块 ---- */
#include "peripheral_ctrl.h"
#include "odrive_uart.h"
#include "ble_uart.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 包含生成的RGB565图片 - 机器人模式
#include "idle_rgb565.h"
#include "haqi_rgb565.h"
#include "angry_rgb565.h"
#include "closeeye_rgb565.h"
#include "smile_rgb565.h"

// 包含YNF模式图片
#include "ynfidle_rgb565.h"
#include "ynfhaqi_rgb565.h"
#include "ynfangry_rgb565.h"

/* ============================================================
 *  BLE RX 回调: 将手机发来的数据透传到 UART0 (TXD0/RXD0)
 * ============================================================ */
static void on_ble_rx(const uint8_t *data, size_t len)
{
    uart_write_bytes(UART_NUM_0, (const char *)data, (int)len);
}

/* ============================================================
 *  UART0 RX -> BLE 透传任务
 *  持续读取 RXD0 上的数据并转发给已连接的 BLE 客户端
 *  注意: buf 用 static 避免占满任务栈
 * ============================================================ */
#define UART0_BRIDGE_BUF_SIZE  512
static void uart0_to_ble_task(void *arg)
{
    static uint8_t buf[UART0_BRIDGE_BUF_SIZE];
    while (1) {
        int len = uart_read_bytes(UART_NUM_0, buf, sizeof(buf),
                                  pdMS_TO_TICKS(20));
        if (len > 0 && ble_uart_is_connected()) {
            ble_uart_send(buf, (size_t)len);
        }
    }
}

/* ============================================================
 *  按键扫描任务 (50ms 轮询)
 *  KEY1: 发送 M0 停止命令
 *  KEY2: 发送 M1 停止命令
 *  KEY3: 清除 M0/M1 错误
 *  KEY4: 播放提示音
 * ============================================================ */
static void key_scan_task(void *arg)
{
    bool prev[4] = {false, false, false, false};
    while (1) {
        for (uint8_t i = 1; i <= 4; i++) {
            bool cur = key_get(i);
            /* 上升沿触发（按下） */
            if (cur && !prev[i - 1]) {
                switch (i) {
                case 1:
                    odrive_uart_send_cmd("$M0,H,0;\n");
                    printf("[KEY1] M0 急停\n");
                    break;
                case 2:
                    odrive_uart_send_cmd("$M1,H,0;\n");
                    printf("[KEY2] M1 急停\n");
                    break;
                case 3:
                    odrive_uart_send_cmd("$M0,E,0;\n");
                    odrive_uart_send_cmd("$M1,E,0;\n");
                    printf("[KEY3] 清除错误\n");
                    break;
                case 4:
                    audio_beep();
                    printf("[KEY4] 提示音\n");
                    break;
                }
            }
            prev[i - 1] = cur;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    /* 只保留 WARN 及以上级别，屏蔽 SERVO/LCD/Touch 等 INFO 噪声 */
    esp_log_level_set("*", ESP_LOG_WARN);

    /* -------- 必须在 BLE/WiFi 之前初始化 NVS -------- */
    /* NVS 未初始化会导致 BLE 射频校准失败，协议栈不稳定甚至断连 */
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    printf("\n\n========================================\n");
    printf("ESP32-S3 HKR Robot Middleware\n");
    printf("========================================\n");
    
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("Chip: %s with %d CPU cores, WiFi%s%s\n",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
    
    uint32_t flash_size;
    esp_flash_get_size(NULL, &flash_size);
    printf("Flash: %lu MB\n", flash_size / (1024 * 1024));
    printf("========================================\n\n");

    /* -------- 初始化外设控制 (LED / KEY / 音频 / 备用UART) -------- */
    printf("Initializing peripheral control...\n");
    if (peripheral_ctrl_init() == ESP_OK) {
        printf("Peripheral control OK\n");
        audio_beep();   /* 上电提示音 */
    } else {
        printf("Peripheral control init FAILED\n");
    }

    /* -------- 初始化 BLE UART (Nordic UART Service) -------- */
    printf("Initializing BLE UART...\n");
    if (ble_uart_init(on_ble_rx) == ESP_OK) {
        printf("BLE UART (NUS) OK, advertising as 'HKR_Robot'\n");
    } else {
        printf("BLE UART init FAILED\n");
    }

    /* -------- 安装 UART0 驱动，启动 UART0<->BLE 透传任务 -------- */
    /* GPIO43=TXD0, GPIO44=RXD0, 115200 baud
     * RX buf=4096 应对 10ms 高频数据爆发。
     * TX buf=2048 确保 on_ble_rx 将数据拷贝到缓冲并立刻返回，
     *   避免阻塞 NimBLE 宿主任务导致 BLE 断连。 */
    uart_driver_install(UART_NUM_0, 4096, 2048, 0, NULL, 0);
    /* 关键：让 VFS (printf/scanf) 通过驱动接口访问 UART0，
     * 避免 ROM UART ISR 与驱动 ISR 并存导致收到数据时 panic 重启 */
    uart_vfs_dev_use_driver(0);
    /* 提高任务栈到 4096，防止栈溢出 */
    xTaskCreate(uart0_to_ble_task, "u0_ble_bridge", 4096, NULL, 5, NULL);
    printf("UART0<->BLE transparent bridge started (GPIO43=TXD0, GPIO44=RXD0)\n");

    /* -------- 初始化 ODrive UART (BLE转发暂时禁用，专注UART0透传测试) -------- */
    printf("Initializing ODrive UART (IO%d TX, IO%d RX)...\n",
           ODRIVE_TX_PIN, ODRIVE_RX_PIN);
    if (odrive_uart_init(NULL) == ESP_OK) {
        printf("ODrive UART OK\n");
    } else {
        printf("ODrive UART init FAILED\n");
    }

    /* -------- 启动按键扫描任务 -------- */
    xTaskCreate(key_scan_task, "key_scan", 2048, NULL, 3, NULL);

    /* LCD 在 BLE 透传测试阶段禁用，避免 SPI 日志噪声与资源占用 */
    // LCD_Init();
    // LCD_BLK_Set();

    /* BLE透传测试模式：不启动触摸/舵机/表情/LCD刷新任务 */

    // app_main 不能返回，需要保持运行
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}