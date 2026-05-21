#pragma once

#include <stdint.h>
#include <stddef.h>
#include "driver/i2s.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

class Es8388AudioCodec {
public:
    // simplified constructor: i2c not used in this stub, pass sample rates and i2s pins
    Es8388AudioCodec(void* i2c_handle, i2c_port_t i2c_port, int sample_rate_in, int sample_rate_out,
                     gpio_num_t mclk_pin, gpio_num_t sck_pin, gpio_num_t ws_pin, gpio_num_t sdout_pin, gpio_num_t sdin_pin,
                     gpio_num_t pa_enable_gpio, uint8_t i2c_addr);
    ~Es8388AudioCodec();

    esp_err_t Init();
    void EnableOutput(bool en);
    void EnableInput(bool en);
    // blocking write/read of 16-bit PCM samples (interleaved if stereo)
    int Write(const int16_t* data, size_t samples);
    int Read(int16_t* data, size_t samples);

private:
    i2s_port_t i2s_port;
    i2c_port_t i2c_port;
    uint8_t i2c_addr;
    i2s_config_t i2s_cfg;
    i2s_pin_config_t pin_cfg;
    int sample_rate;
    gpio_num_t pa_gpio;
    
    // I2C register write helper
    esp_err_t WriteReg(uint8_t reg, uint8_t value);
    esp_err_t ReadReg(uint8_t reg, uint8_t* value);
};
