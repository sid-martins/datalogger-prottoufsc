idf_component_register(
    SRCS "src/datalogger_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES hal_interfaces ring_buffer
)

