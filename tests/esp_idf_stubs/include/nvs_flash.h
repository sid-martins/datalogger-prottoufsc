#pragma once

#include "esp_err.h"
#include "nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline esp_err_t nvs_flash_init(void) {
    return ESP_OK;
}

static inline esp_err_t nvs_flash_init_partition(const char* partition_label) {
    return (partition_label == 0) ? ESP_ERR_INVALID_ARG : ESP_OK;
}

static inline esp_err_t nvs_flash_erase(void) {
    return ESP_OK;
}

static inline esp_err_t nvs_flash_erase_partition(const char* partition_label) {
    return (partition_label == 0) ? ESP_ERR_INVALID_ARG : ESP_OK;
}

#ifdef __cplusplus
}
#endif
