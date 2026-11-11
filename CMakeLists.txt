cmake_minimum_required(VERSION 3.16)

# Se estivermos compilando pelo ESP-IDF (idf.py build), usamos o fluxo oficial da Espressif.
# Caso contrário (cmake -B build_tests), compilamos a suíte de testes TDD (Google Test) no Host (PC).
if(DEFINED ENV{IDF_PATH} AND NOT BUILD_HOST_TESTS)
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(datalogger_esp32s3)
else()
    project(datalogger_host_tests LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    enable_testing()
    add_subdirectory(tests)
endif()

