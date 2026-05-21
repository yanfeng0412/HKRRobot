#ifndef BLE_UART_H
#define BLE_UART_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief BLE Nordic UART Service (NUS) 实现
 *
 * 使用 NimBLE 协议栈，广播名称 "HKR_Robot"。
 *
 * NUS UUIDs (128-bit):
 *   Service : 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 *   RX Char : 6E400002-B5A3-F393-E0A9-E50E24DCCA9E  (手机->ESP32, Write)
 *   TX Char : 6E400003-B5A3-F393-E0A9-E50E24DCCA9E  (ESP32->手机, Notify)
 *
 * 数据流向：
 *   ODrive -> UART -> odrive_uart.c -> ble_uart_send() -> 手机 BLE客户端
 *   手机 BLE客户端 -> RX Char -> ble_rx_cb -> odrive_uart_send_cmd()
 */

/** 收到手机发来的 BLE 数据时调用的回调（字节流，无 '\0' 保证） */
typedef void (*ble_uart_rx_cb_t)(const uint8_t *data, size_t len);

/**
 * @brief 初始化 NimBLE 协议栈并启动广播
 * @param rx_cb  收到 BLE RX 数据时的回调（转发给 ODrive UART）
 * @return ESP_OK 成功
 */
esp_err_t ble_uart_init(ble_uart_rx_cb_t rx_cb);

/**
 * @brief 向已连接的 BLE 中心设备发送数据 (Notify)
 * @param data  数据指针
 * @param len   数据长度
 * @return ESP_OK 成功；ESP_ERR_INVALID_STATE 当前无连接
 */
esp_err_t ble_uart_send(const uint8_t *data, size_t len);

/** @brief 当前是否有 BLE 设备连接 */
bool ble_uart_is_connected(void);

#endif /* BLE_UART_H */
