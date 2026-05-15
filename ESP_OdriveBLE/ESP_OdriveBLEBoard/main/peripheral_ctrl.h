#ifndef PERIPHERAL_CTRL_H
#define PERIPHERAL_CTRL_H

#include "esp_err.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ========== LED引脚 (GPIO输出，低电平OFF) ========== */
#define LED1_GPIO   5
#define LED2_GPIO   6
#define LED3_GPIO   7
#define LED4_GPIO   4

/* ========== 按键引脚 (常低，按下上拉至3.3V) ========== */
#define KEY1_GPIO   38
#define KEY2_GPIO   45
#define KEY3_GPIO   46
#define KEY4_GPIO   3

/* ========== 音频PWM (NS4150功放，IO42) ========== */
#define AUDIO_PWM_GPIO      42
#define AUDIO_PWM_TIMER     LEDC_TIMER_1
#define AUDIO_PWM_CHANNEL   LEDC_CHANNEL_1
#define AUDIO_PWM_RES_BITS  LEDC_TIMER_10_BIT   /* 10位分辨率，占空比0-1023 */

/* ========== 备用UART (IO1=RX, IO2=TX) ========== */
#define BACKUP_UART_NUM     UART_NUM_2
#define BACKUP_UART_RX_PIN  1
#define BACKUP_UART_TX_PIN  2
#define BACKUP_UART_BAUD    115200
#define BACKUP_UART_BUF     256

/* ========== SD卡引脚 (预留，SPI接口) ========== */
#define SD_CS_GPIO    10
#define SD_MOSI_GPIO  11
#define SD_CLK_GPIO   12
#define SD_MISO_GPIO  13

/**
 * @brief 初始化所有外设 (LED/KEY/音频PWM/备用UART)
 */
esp_err_t peripheral_ctrl_init(void);

/* ---------- LED控制 ---------- */
/** @brief 设置LED状态  led_num: 1-4,  on: true=亮 */
void led_set(uint8_t led_num, bool on);
/** @brief 翻转LED状态 */
void led_toggle(uint8_t led_num);
/** @brief 关闭全部LED */
void led_all_off(void);

/* ---------- 按键读取 ---------- */
/** @brief 读取按键电平  key_num: 1-4,  返回 true=按下(高电平) */
bool key_get(uint8_t key_num);

/* ---------- 音频PWM ---------- */
/** @brief 初始化音频PWM (LEDC Timer1/Channel1) */
esp_err_t audio_pwm_init(void);
/** @brief 播放指定频率的音调 (阻塞 duration_ms 毫秒后自动停止；duration_ms=0 则持续播放) */
void audio_play_tone(uint32_t freq_hz, uint32_t duration_ms);
/** @brief 停止音频输出 */
void audio_stop(void);
/** @brief 播放短促提示音 (1kHz, 100ms) */
void audio_beep(void);

/* ---------- 备用UART ---------- */
/** @brief 初始化备用UART (IO1=RX, IO2=TX, 115200 8N1) */
esp_err_t backup_uart_init(void);
/** @brief 向备用UART发送数据 */
int backup_uart_write(const uint8_t *data, size_t len);
/** @brief 从备用UART读取数据 (非阻塞，返回实际读取字节数) */
int backup_uart_read(uint8_t *buf, size_t max_len, uint32_t timeout_ms);

#endif /* PERIPHERAL_CTRL_H */
