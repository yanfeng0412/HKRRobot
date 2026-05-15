#include "servo_control.h"
#include <math.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "SERVO";

// 舵机状态
static float current_angle = 90.0f;  // 当前角度
static bool servo_initialized = false;
static TaskHandle_t random_motion_task_handle = NULL;

// 随机运动参数
typedef struct {
    float min_angle;
    float max_angle;
    uint32_t min_duration_ms;
    uint32_t max_duration_ms;
    uint32_t min_pause_ms;
    uint32_t max_pause_ms;
} random_motion_params_t;

static random_motion_params_t motion_params;

/**
 * @brief 将角度转换为PWM占空比
 */
static uint32_t angle_to_duty(float angle)
{
    // 限制角度范围
    if (angle < SERVO_MIN_ANGLE) angle = SERVO_MIN_ANGLE;
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;
    
    // 计算脉宽（微秒）
    float pulsewidth_us = SERVO_MIN_PULSEWIDTH_US + 
                         (angle / SERVO_MAX_ANGLE) * 
                         (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US);
    
    // 转换为占空比
    // duty = (pulsewidth_us / period_us) * max_duty
    // period_us = 1000000 / 50 = 20000us
    uint32_t max_duty = (1 << SERVO_PWM_RESOLUTION) - 1;
    uint32_t duty = (uint32_t)((pulsewidth_us / 20000.0f) * max_duty);
    
    return duty;
}

/**
 * @brief 三次样条插值函数（S曲线加减速）
 * @param t 时间比例 (0-1)
 * @return 插值结果 (0-1)
 */
static float cubic_spline_interpolation(float t)
{
    // 限制t在[0,1]范围内
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    
    // 使用平滑的S曲线：3t² - 2t³
    // 这个曲线在起点和终点速度都为0，中间加速度平滑变化
    return 3.0f * t * t - 2.0f * t * t * t;
}

esp_err_t servo_init(void)
{
    if (servo_initialized) {
        ESP_LOGW(TAG, "Servo already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing servo on GPIO %d", SERVO_GPIO);
    
    // 配置LEDC定时器
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = SERVO_PWM_RESOLUTION,
        .timer_num = SERVO_PWM_TIMER,
        .freq_hz = SERVO_PWM_FREQ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    
    esp_err_t ret = ledc_timer_config(&timer_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 配置LEDC通道
    ledc_channel_config_t channel_config = {
        .gpio_num = SERVO_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = SERVO_PWM_CHANNEL,
        .timer_sel = SERVO_PWM_TIMER,
        .duty = angle_to_duty(90.0f),  // 初始位置在中间
        .hpoint = 0,
        .flags.output_invert = 0
    };
    
    ret = ledc_channel_config(&channel_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    current_angle = 90.0f;
    servo_initialized = true;
    
    ESP_LOGI(TAG, "Servo initialized successfully at 90 degrees");
    return ESP_OK;
}

esp_err_t servo_set_angle(float angle)
{
    if (!servo_initialized) {
        ESP_LOGE(TAG, "Servo not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 限制角度范围
    if (angle < SERVO_MIN_ANGLE) angle = SERVO_MIN_ANGLE;
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;
    
    uint32_t duty = angle_to_duty(angle);
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_PWM_CHANNEL, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set duty: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_PWM_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update duty: %s", esp_err_to_name(ret));
        return ret;
    }
    
    current_angle = angle;
    return ESP_OK;
}

esp_err_t servo_move_smooth(float target_angle, uint32_t duration_ms)
{
    if (!servo_initialized) {
        ESP_LOGE(TAG, "Servo not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 限制目标角度范围
    if (target_angle < SERVO_MIN_ANGLE) target_angle = SERVO_MIN_ANGLE;
    if (target_angle > SERVO_MAX_ANGLE) target_angle = SERVO_MAX_ANGLE;
    
    float start_angle = current_angle;
    float angle_diff = target_angle - start_angle;
    
    // 如果角度差很小，直接设置
    if (fabs(angle_diff) < 0.5f) {
        return servo_set_angle(target_angle);
    }
    
    ESP_LOGI(TAG, "Moving from %.1f° to %.1f° in %lu ms", start_angle, target_angle, duration_ms);
    
    uint32_t update_interval_ms = 20;  // 50Hz更新频率
    uint32_t steps = duration_ms / update_interval_ms;
    
    if (steps < 1) steps = 1;
    
    int64_t start_time = esp_timer_get_time();
    
    for (uint32_t i = 0; i <= steps; i++) {
        // 计算当前时间比例
        float t = (float)i / (float)steps;
        
        // 应用三次样条插值
        float interpolated = cubic_spline_interpolation(t);
        
        // 计算当前角度
        float current = start_angle + angle_diff * interpolated;
        
        // 设置角度
        esp_err_t ret = servo_set_angle(current);
        if (ret != ESP_OK) {
            return ret;
        }
        
        // 等待到下一个更新时间点
        if (i < steps) {
            int64_t target_time = start_time + (i + 1) * update_interval_ms * 1000;
            int64_t current_time = esp_timer_get_time();
            int64_t delay_us = target_time - current_time;
            
            if (delay_us > 0) {
                vTaskDelay(pdMS_TO_TICKS(delay_us / 1000));
            }
        }
    }
    
    ESP_LOGI(TAG, "Movement completed at %.1f°", target_angle);
    return ESP_OK;
}

float servo_get_current_angle(void)
{
    return current_angle;
}

/**
 * @brief 随机运动任务
 */
static void servo_random_motion_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Random motion task started");
    ESP_LOGI(TAG, "Angle range: %.1f° - %.1f°", motion_params.min_angle, motion_params.max_angle);
    ESP_LOGI(TAG, "Duration range: %lu - %lu ms", motion_params.min_duration_ms, motion_params.max_duration_ms);
    ESP_LOGI(TAG, "Pause range: %lu - %lu ms", motion_params.min_pause_ms, motion_params.max_pause_ms);
    
    // 初始化随机数种子
    srand(esp_timer_get_time());
    
    while (1) {
        // 生成随机目标角度
        float angle_range = motion_params.max_angle - motion_params.min_angle;
        float target_angle = motion_params.min_angle + ((float)rand() / RAND_MAX) * angle_range;
        
        // 生成随机运动时间
        uint32_t duration_range = motion_params.max_duration_ms - motion_params.min_duration_ms;
        uint32_t duration = motion_params.min_duration_ms + (rand() % (duration_range + 1));
        
        // 生成随机停顿时间
        uint32_t pause_range = motion_params.max_pause_ms - motion_params.min_pause_ms;
        uint32_t pause = motion_params.min_pause_ms + (rand() % (pause_range + 1));
        
        ESP_LOGI(TAG, "Moving to %.1f° (duration: %lu ms, pause: %lu ms)", 
                 target_angle, duration, pause);
        
        // 平滑移动到目标位置
        servo_move_smooth(target_angle, duration);
        
        // 停顿
        vTaskDelay(pdMS_TO_TICKS(pause));
    }
}

esp_err_t servo_start_random_motion(float min_angle, float max_angle,
                                    uint32_t min_duration_ms, uint32_t max_duration_ms,
                                    uint32_t min_pause_ms, uint32_t max_pause_ms)
{
    if (!servo_initialized) {
        ESP_LOGE(TAG, "Servo not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (random_motion_task_handle != NULL) {
        ESP_LOGW(TAG, "Random motion task already running");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 验证参数
    if (min_angle < SERVO_MIN_ANGLE) min_angle = SERVO_MIN_ANGLE;
    if (max_angle > SERVO_MAX_ANGLE) max_angle = SERVO_MAX_ANGLE;
    if (min_angle >= max_angle) {
        ESP_LOGE(TAG, "Invalid angle range");
        return ESP_ERR_INVALID_ARG;
    }
    if (min_duration_ms >= max_duration_ms) {
        ESP_LOGE(TAG, "Invalid duration range");
        return ESP_ERR_INVALID_ARG;
    }
    if (min_pause_ms >= max_pause_ms) {
        ESP_LOGE(TAG, "Invalid pause range");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 保存参数
    motion_params.min_angle = min_angle;
    motion_params.max_angle = max_angle;
    motion_params.min_duration_ms = min_duration_ms;
    motion_params.max_duration_ms = max_duration_ms;
    motion_params.min_pause_ms = min_pause_ms;
    motion_params.max_pause_ms = max_pause_ms;
    
    // 创建随机运动任务（优先级1，避免抢占UI和触摸检测）
    BaseType_t ret = xTaskCreate(
        servo_random_motion_task,
        "servo_random",
        4096,
        NULL,
        1,
        &random_motion_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create random motion task");
        random_motion_task_handle = NULL;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Random motion task started successfully");
    return ESP_OK;
}

void servo_stop_random_motion(void)
{
    if (random_motion_task_handle != NULL) {
        vTaskDelete(random_motion_task_handle);
        random_motion_task_handle = NULL;
        ESP_LOGI(TAG, "Random motion task stopped");
    }
}
