idf_component_register(
    SRCS "src/can_twai_lld.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer log bsp hal
)

