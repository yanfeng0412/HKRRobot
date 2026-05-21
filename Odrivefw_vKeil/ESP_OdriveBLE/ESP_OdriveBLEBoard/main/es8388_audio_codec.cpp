#include "es8388_audio_codec.h"
#include <string.h>
#include "esp_log.h"

static const char* TAG = "ES8388";

// ES8388 Register definitions
#define ES8388_CONTROL1         0x00
#define ES8388_CONTROL2         0x01
#define ES8388_CHIPPOWER        0x02
#define ES8388_ADCPOWER         0x03
#define ES8388_DACPOWER         0x04
#define ES8388_CHIPLOPOW1       0x05
#define ES8388_CHIPLOPOW2       0x06
#define ES8388_ANAVOLMANAG      0x07
#define ES8388_MASTERMODE       0x08
#define ES8388_ADCCONTROL1      0x09
#define ES8388_ADCCONTROL2      0x0A
#define ES8388_ADCCONTROL3      0x0B
#define ES8388_ADCCONTROL4      0x0C
#define ES8388_ADCCONTROL5      0x0D
#define ES8388_ADCCONTROL6      0x0E
#define ES8388_ADCCONTROL7      0x0F
#define ES8388_ADCCONTROL8      0x10
#define ES8388_ADCCONTROL9      0x11
#define ES8388_ADCCONTROL10     0x12
#define ES8388_ADCCONTROL11     0x13
#define ES8388_ADCCONTROL12     0x14
#define ES8388_ADCCONTROL13     0x15
#define ES8388_ADCCONTROL14     0x16
#define ES8388_DACCONTROL1      0x17
#define ES8388_DACCONTROL2      0x18
#define ES8388_DACCONTROL3      0x19
#define ES8388_DACCONTROL4      0x1A
#define ES8388_DACCONTROL5      0x1B
#define ES8388_DACCONTROL6      0x1C
#define ES8388_DACCONTROL7      0x1D
#define ES8388_DACCONTROL8      0x1E
#define ES8388_DACCONTROL9      0x1F
#define ES8388_DACCONTROL10     0x20
#define ES8388_DACCONTROL11     0x21
#define ES8388_DACCONTROL12     0x22
#define ES8388_DACCONTROL13     0x23
#define ES8388_DACCONTROL14     0x24
#define ES8388_DACCONTROL15     0x25
#define ES8388_DACCONTROL16     0x26
#define ES8388_DACCONTROL17     0x27
#define ES8388_DACCONTROL18     0x28
#define ES8388_DACCONTROL19     0x29
#define ES8388_DACCONTROL20     0x2A
#define ES8388_DACCONTROL21     0x2B
#define ES8388_DACCONTROL22     0x2C
#define ES8388_DACCONTROL23     0x2D
#define ES8388_DACCONTROL24     0x2E
#define ES8388_DACCONTROL25     0x2F
#define ES8388_DACCONTROL26     0x30
#define ES8388_DACCONTROL27     0x31
#define ES8388_DACCONTROL28     0x32
#define ES8388_DACCONTROL29     0x33
#define ES8388_DACCONTROL30     0x34

Es8388AudioCodec::Es8388AudioCodec(void* /*i2c_handle*/, i2c_port_t i2c_port_in, int sample_rate_in, int /*sample_rate_out*/,
                                   gpio_num_t mclk_pin, gpio_num_t sck_pin, gpio_num_t ws_pin, gpio_num_t sdout_pin, gpio_num_t sdin_pin,
                                   gpio_num_t pa_enable_gpio, uint8_t i2c_addr_in)
: i2s_port(I2S_NUM_0), i2c_port(i2c_port_in), i2c_addr(i2c_addr_in), sample_rate(sample_rate_in), pa_gpio(pa_enable_gpio)
{
    memset(&i2s_cfg, 0, sizeof(i2s_cfg));
    i2s_cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
    i2s_cfg.sample_rate = sample_rate;
    i2s_cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT; // mono for simplicity
    i2s_cfg.communication_format = I2S_COMM_FORMAT_STAND_MSB;
    i2s_cfg.dma_buf_count = 4;
    i2s_cfg.dma_buf_len = 256;
    i2s_cfg.use_apll = false;
    i2s_cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;  // 降低中断优先级，避免干扰SPI
    i2s_cfg.fixed_mclk = 0;  // 让系统自动计算MCLK
    i2s_cfg.tx_desc_auto_clear = true;  // 自动清除TX描述符

    pin_cfg.mck_io_num = I2S_PIN_NO_CHANGE;  // 禁用MCLK输出，避免干扰
    pin_cfg.bck_io_num = sck_pin;
    pin_cfg.ws_io_num = ws_pin;
    pin_cfg.data_out_num = sdout_pin;
    pin_cfg.data_in_num = sdin_pin;
}

Es8388AudioCodec::~Es8388AudioCodec() {
    i2s_driver_uninstall(i2s_port);
}

esp_err_t Es8388AudioCodec::WriteReg(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Write reg 0x%02x failed: %d", reg, ret);
    }
    return ret;
}

esp_err_t Es8388AudioCodec::ReadReg(uint8_t reg, uint8_t* value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t Es8388AudioCodec::Init() {
    ESP_LOGI(TAG, "Initializing ES8388 codec at I2C address 0x%02x", i2c_addr);
    
    // Always configure I2S first (even if ES8388 I2C fails, I2S can work standalone)
    esp_err_t err = i2s_driver_install(i2s_port, &i2s_cfg, 0, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_driver_install failed: %d", err);
        return err;
    }
    i2s_set_pin(i2s_port, &pin_cfg);
    
    if (pa_gpio != GPIO_NUM_NC) {
        gpio_set_direction(pa_gpio, GPIO_MODE_OUTPUT);
        gpio_set_level(pa_gpio, 0);
    }
    
    ESP_LOGI(TAG, "I2S driver installed @ %d Hz", sample_rate);
    
    // Test I2C communication by reading chip ID registers
    uint8_t chip_id1 = 0, chip_id2 = 0;
    esp_err_t read_result = ReadReg(0x00, &chip_id1);
    if (read_result == ESP_OK) {
        ReadReg(0x01, &chip_id2);
        ESP_LOGI(TAG, "ES8388 detected: ID1=0x%02x, ID2=0x%02x", chip_id1, chip_id2);
    } else {
        ESP_LOGW(TAG, "Cannot communicate with ES8388 at address 0x%02x - continuing with I2S only", i2c_addr);
        ESP_LOGW(TAG, "Audio capture may not work without ES8388 configuration");
        return ESP_OK; // Return OK since I2S is installed, just ES8388 config failed
    }
    
    // Reset and power up ES8388
    ESP_LOGI(TAG, "Resetting ES8388...");
    WriteReg(ES8388_CONTROL1, 0x80);  // Reset
    vTaskDelay(pdMS_TO_TICKS(100));
    WriteReg(ES8388_CONTROL1, 0x00);  // Clear reset
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Power management
    WriteReg(ES8388_CHIPPOWER, 0x00);     // Power up everything
    WriteReg(ES8388_MASTERMODE, 0x00);    // Slave mode (ESP32 is master)
    
    // Analog power management
    WriteReg(ES8388_ANAVOLMANAG, 0x7C);   // Enable analog inputs
    
    // ADC Power and Configuration
    WriteReg(ES8388_ADCPOWER, 0x00);      // Power up ADC (both channels)
    WriteReg(ES8388_ADCCONTROL1, 0x88);   // Mic PGA gain = +24dB (LIN1: bits 7-4, RIN1: bits 3-0)
    WriteReg(ES8388_ADCCONTROL2, 0xF0);   // Input Select: LIN1/RIN1 differential input
    WriteReg(ES8388_ADCCONTROL3, 0x02);   // ADC format: I2S, 16-bit
    WriteReg(ES8388_ADCCONTROL4, 0x0C);   // I2S format, 16-bit
    WriteReg(ES8388_ADCCONTROL5, 0x02);   // ADC to I2S routing
    WriteReg(ES8388_ADCCONTROL8, 0x00);   // ADC Volume L = 0dB
    WriteReg(ES8388_ADCCONTROL9, 0x00);   // ADC Volume R = 0dB
    WriteReg(ES8388_ADCCONTROL10, 0x3A);  // ADC digital filtering
    
    // Enable microphone bias
    WriteReg(0x0B, 0x02);                 // Enable MICBIAS (bit 1)
    
    ESP_LOGI(TAG, "ADC configured for microphone input on LIN1/RIN1");
    
    // DAC Configuration
    WriteReg(ES8388_DACPOWER, 0x00);      // Power up DAC
    WriteReg(ES8388_DACCONTROL1, 0x18);   // I2S 16-bit
    WriteReg(ES8388_DACCONTROL2, 0x02);   // DAC control
    WriteReg(ES8388_DACCONTROL3, 0x00);   // DAC Volume = 0dB
    WriteReg(ES8388_DACCONTROL4, 0x00);   // DAC Volume L = 0dB
    WriteReg(ES8388_DACCONTROL5, 0x00);   // DAC Volume R = 0dB
    WriteReg(ES8388_DACCONTROL17, 0xB8);  // LOUT1/ROUT1 volume = 0dB
    WriteReg(ES8388_DACCONTROL20, 0xB8);  // LOUT2/ROUT2 volume = 0dB
    
    // Enable outputs
    WriteReg(ES8388_CHIPLOPOW1, 0x00);    // Low power mode off
    WriteReg(ES8388_CHIPLOPOW2, 0x00);    // Low power mode off
    
    ESP_LOGI(TAG, "ES8388 registers fully configured");
    ESP_LOGI(TAG, "ES8388 AudioCodec initialized @ %d Hz", sample_rate);
    return ESP_OK;
}

void Es8388AudioCodec::EnableOutput(bool en) {
    if (pa_gpio != GPIO_NUM_NC) gpio_set_level(pa_gpio, en ? 1 : 0);
}

void Es8388AudioCodec::EnableInput(bool en) {
    // nothing to do for stub
    (void)en;
}

int Es8388AudioCodec::Write(const int16_t* data, size_t samples) {
    if (!data || samples == 0) return 0;
    size_t bytes_written = 0;
    i2s_write(i2s_port, (const char*)data, samples * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    return (int)(bytes_written / sizeof(int16_t));
}

int Es8388AudioCodec::Read(int16_t* data, size_t samples) {
    if (!data || samples == 0) return 0;
    size_t bytes_read = 0;
    esp_err_t res = i2s_read(i2s_port, (char*)data, samples * sizeof(int16_t), &bytes_read, pdMS_TO_TICKS(200));
    if (res != ESP_OK) return 0;
    return (int)(bytes_read / sizeof(int16_t));
}
