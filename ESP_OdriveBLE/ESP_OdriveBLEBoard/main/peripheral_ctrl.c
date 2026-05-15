#include "peripheral_ctrl.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "PERIPH";

static const gpio_num_t s_led_pins[4] = {
    LED1_GPIO, LED2_GPIO, LED3_GPIO, LED4_GPIO
};

static const gpio_num_t s_key_pins[4] = {
    KEY1_GPIO, KEY2_GPIO, KEY3_GPIO, KEY4_GPIO
};

/* ============================================================
 *  初始化
 * ============================================================ */
esp_err_t peripheral_ctrl_init(void)
{
    esp_err_t ret;

    /* --- LED GPIO --- */
    gpio_config_t led_cfg = {
        .pin_bit_mask = ((1ULL << LED1_GPIO) | (1ULL << LED2_GPIO) |
                         (1ULL << LED3_GPIO) | (1ULL << LED4_GPIO)),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&led_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LED GPIO init failed: %d", ret);
        return ret;
    }
    led_all_off();

    /* --- 按键 GPIO (内部下拉，按下时外部上拉至3.3V) --- */
    gpio_config_t key_cfg = {
        .pin_bit_mask = ((1ULL << KEY1_GPIO) | (1ULL << KEY2_GPIO) |
                         (1ULL << KEY3_GPIO) | (1ULL << KEY4_GPIO)),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&key_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "KEY GPIO init failed: %d", ret);
        return ret;
    }

    /* --- 音频PWM --- */
    ret = audio_pwm_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Audio PWM init failed: %d", ret);
        return ret;
    }

    /* --- 备用UART --- */
    ret = backup_uart_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Backup UART init failed: %d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "LED(IO%d/%d/%d/%d) KEY(IO%d/%d/%d/%d) AudioPWM(IO%d) BackupUART(RX=IO%d TX=IO%d) OK",
             LED1_GPIO, LED2_GPIO, LED3_GPIO, LED4_GPIO,
             KEY1_GPIO, KEY2_GPIO, KEY3_GPIO, KEY4_GPIO,
             AUDIO_PWM_GPIO, BACKUP_UART_RX_PIN, BACKUP_UART_TX_PIN);
    return ESP_OK;
}

/* ============================================================
 *  LED
 * ============================================================ */
void led_set(uint8_t led_num, bool on)
{
    if (led_num < 1 || led_num > 4) return;
    gpio_set_level(s_led_pins[led_num - 1], on ? 1 : 0);
}

void led_toggle(uint8_t led_num)
{
    if (led_num < 1 || led_num > 4) return;
    int level = gpio_get_level(s_led_pins[led_num - 1]);
    gpio_set_level(s_led_pins[led_num - 1], level ? 0 : 1);
}

void led_all_off(void)
{
    for (int i = 0; i < 4; i++) {
        gpio_set_level(s_led_pins[i], 0);
    }
}

/* ============================================================
 *  按键
 * ============================================================ */
bool key_get(uint8_t key_num)
{
    if (key_num < 1 || key_num > 4) return false;
    return (gpio_get_level(s_key_pins[key_num - 1]) == 1);
}

/* ============================================================
 *  音频 PWM (LEDC Timer1 / Channel1)
 * ============================================================ */
esp_err_t audio_pwm_init(void)
{
    ledc_timer_config_t tim_cfg = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = AUDIO_PWM_TIMER,
        .duty_resolution = AUDIO_PWM_RES_BITS,
        .freq_hz         = 1000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t ret = ledc_timer_config(&tim_cfg);
    if (ret != ESP_OK) return ret;

    ledc_channel_config_t ch_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = AUDIO_PWM_CHANNEL,
        .timer_sel  = AUDIO_PWM_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = AUDIO_PWM_GPIO,
        .duty       = 0,
        .hpoint     = 0,
    };
    return ledc_channel_config(&ch_cfg);
}

void audio_play_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    if (freq_hz == 0) {
        audio_stop();
        return;
    }
    ledc_set_freq(LEDC_LOW_SPEED_MODE, AUDIO_PWM_TIMER, freq_hz);
    /* 50% 占空比 = 512 (10位分辨率) */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, AUDIO_PWM_CHANNEL, 512);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, AUDIO_PWM_CHANNEL);

    if (duration_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        audio_stop();
    }
}

void audio_stop(void)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, AUDIO_PWM_CHANNEL, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, AUDIO_PWM_CHANNEL);
}

void audio_beep(void)
{
    audio_play_tone(1000, 100);
}

/* ============================================================
 *  备用 UART (UART2: IO1=RX, IO2=TX)
 * ============================================================ */
esp_err_t backup_uart_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = BACKUP_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_driver_install(BACKUP_UART_NUM,
                                        BACKUP_UART_BUF * 2,
                                        BACKUP_UART_BUF * 2,
                                        0, NULL, 0);
    if (ret != ESP_OK) return ret;

    ret = uart_param_config(BACKUP_UART_NUM, &cfg);
    if (ret != ESP_OK) return ret;

    return uart_set_pin(BACKUP_UART_NUM,
                        BACKUP_UART_TX_PIN,
                        BACKUP_UART_RX_PIN,
                        UART_PIN_NO_CHANGE,
                        UART_PIN_NO_CHANGE);
}

int backup_uart_write(const uint8_t *data, size_t len)
{
    return uart_write_bytes(BACKUP_UART_NUM, (const char *)data, len);
}

int backup_uart_read(uint8_t *buf, size_t max_len, uint32_t timeout_ms)
{
    return uart_read_bytes(BACKUP_UART_NUM, buf, max_len,
                           pdMS_TO_TICKS(timeout_ms));
}
