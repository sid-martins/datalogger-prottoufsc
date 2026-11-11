cmake_minimum_required(VERSION 3.16)
project(datalogger_tests LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# No Windows, evita sobrescrever as flags de runtime (/MD vs /MT) do projeto pai
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Se o GoogleTest já estiver presente localmente em tests/external/googletest,
# usa a cópia local (funcionando 100% offline); caso contrário, baixa via FetchContent.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/googletest/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/googletest" "${CMAKE_CURRENT_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(googletest)
endif()

enable_testing()

# Diretórios dos componentes que serão testados no Host (independentes de hardware)
set(COMPONENTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../components")

# Biblioteca reunindo o código de produção (RingBuffer + DataloggerCore) para linkar nos testes
add_library(datalogger_core_lib STATIC
    "${COMPONENTS_DIR}/ring_buffer/src/ring_buffer.cpp"
    "${COMPONENTS_DIR}/datalogger_core/src/datalogger_service.cpp"
)

target_include_directories(datalogger_core_lib PUBLIC
    "${COMPONENTS_DIR}/board_config/include"
    "${COMPONENTS_DIR}/hal_interfaces/include"
    "${COMPONENTS_DIR}/ring_buffer/include"
    "${COMPONENTS_DIR}/datalogger_core/include"
)

target_compile_options(datalogger_core_lib PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# Executável de testes unitários (Google Test + Google Mock)
add_executable(datalogger_unit_tests
    test_board_pins.cpp
    test_ring_buffer.cpp
    test_datalogger_service.cpp
)

target_include_directories(datalogger_unit_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/mocks"
    "${CMAKE_CURRENT_SOURCE_DIR}/esp_idf_stubs/include"
)

target_link_libraries(datalogger_unit_tests PRIVATE
    datalogger_core_lib
    GTest::gtest_main
    GTest::gmock
)

include(GoogleTest)
gtest_discover_tests(datalogger_unit_tests)
