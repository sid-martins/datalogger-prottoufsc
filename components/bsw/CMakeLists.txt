idf_component_register(
    SRCS "src/ring_buffer.cpp" "src/can_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES bsp osal hal
)

