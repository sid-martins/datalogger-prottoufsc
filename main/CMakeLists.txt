idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES freertos esp_timer log driver board_config hal_interfaces ring_buffer can_bus
)
