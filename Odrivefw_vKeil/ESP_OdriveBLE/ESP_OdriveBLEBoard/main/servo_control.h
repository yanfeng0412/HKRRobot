#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

#include "driver/ledc.h"
#include "esp_err.h"

// 舵机配置
#define SERVO_GPIO          41          // 舵机控制引脚
#define SERVO_PWM_FREQ      50          // 舵机频率 50Hz
#define SERVO_PWM_RESOLUTION LEDC_TIMER_14_BIT  // 14位分辨率
#define SERVO_PWM_CHANNEL   LEDC_CHANNEL_0
#define SERVO_PWM_TIMER     LEDC_TIMER_0

// 舵机角度范围
#define SERVO_MIN_ANGLE     0
#define SERVO_MAX_ANGLE     180

// MG90舵机PWM脉宽参数（微秒）
#define SERVO_MIN_PULSEWIDTH_US 500     // 0度对应脉宽
#define SERVO_MAX_PULSEWIDTH_US 2500    // 180度对应脉宽

/**
 * @brief 初始化舵机
 * @return ESP_OK 成功, 其他值失败
 */
esp_err_t servo_init(void);

/**
 * @brief 设置舵机角度（立即设置，无平滑）
 * @param angle 目标角度 (0-180度)
 * @return ESP_OK 成功, 其他值失败
 */
esp_err_t servo_set_angle(float angle);

/**
 * @brief 平滑移动舵机到目标角度（使用三次样条插值）
 * @param target_angle 目标角度 (0-180度)
 * @param duration_ms 运动持续时间（毫秒）
 * @return ESP_OK 成功, 其他值失败
 */
esp_err_t servo_move_smooth(float target_angle, uint32_t duration_ms);

/**
 * @brief 获取当前舵机角度
 * @return 当前角度值
 */
float servo_get_current_angle(void);

/**
 * @brief 启动舵机随机运动任务
 * @param min_angle 最小角度
 * @param max_angle 最大角度
 * @param min_duration_ms 最小运动时间
 * @param max_duration_ms 最大运动时间
 * @param min_pause_ms 最小停顿时间
 * @param max_pause_ms 最大停顿时间
 * @return ESP_OK 成功, 其他值失败
 */
esp_err_t servo_start_random_motion(float min_angle, float max_angle,
                                    uint32_t min_duration_ms, uint32_t max_duration_ms,
                                    uint32_t min_pause_ms, uint32_t max_pause_ms);

/**
 * @brief 停止舵机随机运动任务
 */
void servo_stop_random_motion(void);

#endif // SERVO_CONTROL_H
