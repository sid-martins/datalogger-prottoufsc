cmake_minimum_required(VERSION 3.16)
project(datalogger_sil_tests LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

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

set(COMPONENTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../components")

# Biblioteca reunindo as camadas Motorsport (BSP + OSAL + HAL + LLD + BSW)
add_library(datalogger_bsw_lib STATIC
    "${COMPONENTS_DIR}/bsw/src/ring_buffer.cpp"
    "${COMPONENTS_DIR}/bsw/src/can_service.cpp"
    "${COMPONENTS_DIR}/lld/src/can_twai_lld.cpp"
)

target_include_directories(datalogger_bsw_lib PUBLIC
    "${COMPONENTS_DIR}/bsp/include"
    "${COMPONENTS_DIR}/osal/include"
    "${COMPONENTS_DIR}/hal/include"
    "${COMPONENTS_DIR}/lld/include"
    "${COMPONENTS_DIR}/bsw/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/esp_idf_stubs/include"
)

target_compile_options(datalogger_bsw_lib PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

# Executável de testes SIL (Software-in-the-Loop) com Google Test + Google Mock
add_executable(datalogger_unit_tests
    test_bsp_pins.cpp
    test_ring_buffer.cpp
    test_can_service.cpp
)

target_include_directories(datalogger_unit_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/sil_mocks"
    "${CMAKE_CURRENT_SOURCE_DIR}/esp_idf_stubs/include"
)

target_link_libraries(datalogger_unit_tests PRIVATE
    datalogger_bsw_lib
    GTest::gtest_main
    GTest::gmock
)

include(GoogleTest)
gtest_discover_tests(datalogger_unit_tests)
