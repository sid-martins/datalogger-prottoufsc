idf_component_register(
    SRCS
        "src/can_twai_lld.cpp"
        "src/adc_oneshot_lld.cpp"
        "src/ds3231_i2c_lld.cpp"
        "src/neo8m_uart_lld.cpp"
        "src/sd_card_lld.cpp"
        "src/gpio_lld.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos esp_adc esp_timer log fatfs sdmmc vfs bsp datalogger_hal
)
