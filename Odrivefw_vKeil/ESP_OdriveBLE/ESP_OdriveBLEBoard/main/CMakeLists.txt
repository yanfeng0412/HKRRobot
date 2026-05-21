idf_component_register(
    SRCS
        "servo_control.c"
        "tts_sst_demo.cpp"
        "es8388_audio_codec.cpp"
        "audio_processor.cpp"
        "dify_chat.c"
        "main.c"
        "peripheral_ctrl.c"
        "odrive_uart.c"
        "ble_uart.c"
        "Src/lcd_init.c"
        "Src/lcd.c"
        "Src/cst816.c"
        "Src/iic_hal.c"
        "Src/lcd_demo.c"
        "Src/jpeg_decoder.c"
        "Src/tjpgd.c"
    INCLUDE_DIRS "." "Inc/."
    REQUIRES
        esp_wifi nvs_flash esp_http_client json driver esp_timer bt spi_flash
    EMBED_FILES
        images/angry.jpg images/haqi.jpg images/idle.jpg
)