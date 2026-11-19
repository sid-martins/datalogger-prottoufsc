idf_component_register(
    SRCS
        "src/ring_buffer.cpp"
        "src/can_service.cpp"
        "src/can_decoder_service.cpp"
        "src/adc_service.cpp"
        "src/rtc_service.cpp"
        "src/gps_service.cpp"
        "src/telemetry_aggregator.cpp"
        "src/logger_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES bsp osal hal
)
