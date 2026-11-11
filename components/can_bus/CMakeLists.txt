idf_component_register(
    SRCS "src/can_controller.cpp" "src/esp32_twai_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer log board_config hal_interfaces
)

