#ifndef ODRIVE_UART_H
#define ODRIVE_UART_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ========== ODrive UART 物理层参数 ========== */
#define ODRIVE_UART_NUM      UART_NUM_1
#define ODRIVE_TX_PIN        37       /* ESP32-S3 IO37 -> 电控板 RX */
#define ODRIVE_RX_PIN        36       /* ESP32-S3 IO36 <- 电控板 TX */
#define ODRIVE_BAUD_RATE     115200
#define ODRIVE_RX_BUF_SIZE   2048

/* ========== 解析后的 $ST 状态帧 ========== */
typedef struct {
    float   vbus_v;          /* 母线电压 (V)             */
    int     drv_fault;       /* DRV8301 故障码 (0=正常)  */
    uint8_t m0_hall;         /* M0 Hall 状态 (1-6)       */
    int     m0_count;        /* M0 累计 Hall 计数        */
    float   m0_vel;          /* M0 角速度 (rad/s)        */
    float   m0_iq;           /* M0 Iq 电流 (A)           */
    int     m0_error;        /* M0 错误码                */
    uint8_t m0_status;       /* M0 状态位 bit0=校准 bit1=使能 */
    uint8_t m1_hall;         /* M1 Hall 状态             */
    int     m1_count;        /* M1 累计 Hall 计数        */
    float   m1_vel;          /* M1 角速度 (rad/s)        */
    float   m1_iq;           /* M1 Iq 电流 (A)           */
    int     m1_error;        /* M1 错误码                */
    uint8_t m1_status;       /* M1 状态位                */
    uint32_t timestamp_ms;   /* 接收时间戳               */
    bool    valid;           /* 数据是否有效             */
} odrive_status_t;

/* ========== BLE 转发回调 ========== */
/** 每收到一行 ODrive 数据（包含结尾 '\n'）时调用，用于转发给 BLE */
typedef void (*odrive_ble_cb_t)(const uint8_t *data, size_t len);

/**
 * @brief 初始化 ODrive UART 并启动后台接收任务
 * @param ble_cb  BLE 转发回调（可传 NULL）
 * @return ESP_OK 成功
 */
esp_err_t odrive_uart_init(odrive_ble_cb_t ble_cb);

/**
 * @brief 获取最新解析的状态（线程安全）
 * @param out  目标结构体指针
 */
void odrive_uart_get_status(odrive_status_t *out);

/**
 * @brief 向电控板发送原始命令字符串
 *        命令格式示例: "$M0,V,5.0;\n"
 * @return 实际发送字节数，负数表示失败
 */
int odrive_uart_send_cmd(const char *cmd);

/**
 * @brief 在 LCD 底部状态栏刷新 ODrive 数据（需要 LCD 已初始化）
 *        推荐在主循环或定时器中定期调用（约 2Hz）
 */
void odrive_uart_lcd_update(void);

/**
 * @brief 注册 BLE 转发回调（初始化后也可更换）
 */
void odrive_uart_set_ble_cb(odrive_ble_cb_t cb);

#endif /* ODRIVE_UART_H */
