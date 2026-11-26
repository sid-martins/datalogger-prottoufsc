#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_ERR_NVS_BASE                0x1100
#define ESP_ERR_NVS_NOT_INITIALIZED     (ESP_ERR_NVS_BASE + 0x01)
#define ESP_ERR_NVS_NOT_FOUND           (ESP_ERR_NVS_BASE + 0x02)
#define ESP_ERR_NVS_TYPE_MISMATCH       (ESP_ERR_NVS_BASE + 0x03)
#define ESP_ERR_NVS_READ_ONLY           (ESP_ERR_NVS_BASE + 0x04)
#define ESP_ERR_NVS_NOT_ENOUGH_SPACE    (ESP_ERR_NVS_BASE + 0x05)
#define ESP_ERR_NVS_INVALID_NAME        (ESP_ERR_NVS_BASE + 0x06)
#define ESP_ERR_NVS_INVALID_HANDLE      (ESP_ERR_NVS_BASE + 0x07)
#define ESP_ERR_NVS_REMOVE_FAILED       (ESP_ERR_NVS_BASE + 0x08)
#define ESP_ERR_NVS_KEY_TOO_LONG        (ESP_ERR_NVS_BASE + 0x09)
#define ESP_ERR_NVS_NO_FREE_PAGES       (ESP_ERR_NVS_BASE + 0x0d)
#define ESP_ERR_NVS_INVALID_LENGTH      (ESP_ERR_NVS_BASE + 0x0e)
#define ESP_ERR_NVS_NEW_VERSION_FOUND   (ESP_ERR_NVS_BASE + 0x10)

#define NVS_DEFAULT_PART_NAME           "nvs"
#define NVS_KEY_NAME_MAX_SIZE           16

typedef uint32_t nvs_handle_t;

typedef enum {
    NVS_READONLY  = 0,
    NVS_READWRITE = 1
} nvs_open_mode_t;

static inline esp_err_t nvs_open(const char* name, nvs_open_mode_t open_mode, nvs_handle_t* out_handle) {
    (void)open_mode;
    if (name == 0 || out_handle == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_handle = 1u;
    return ESP_OK;
}

static inline esp_err_t nvs_open_from_partition(const char* part_name,
                                                const char* name,
                                                nvs_open_mode_t open_mode,
                                                nvs_handle_t* out_handle) {
    (void)open_mode;
    if (part_name == 0 || name == 0 || out_handle == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_handle = 1u;
    return ESP_OK;
}

static inline void nvs_close(nvs_handle_t handle) {
    (void)handle;
}

static inline esp_err_t nvs_get_u32(nvs_handle_t handle, const char* key, uint32_t* out_value) {
    if (handle == 0u || key == 0 || out_value == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

static inline esp_err_t nvs_set_u32(nvs_handle_t handle, const char* key, uint32_t value) {
    (void)value;
    if (handle == 0u || key == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static inline esp_err_t nvs_commit(nvs_handle_t handle) {
    return (handle == 0u) ? ESP_ERR_NVS_INVALID_HANDLE : ESP_OK;
}

#ifdef __cplusplus
}
#endif
