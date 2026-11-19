#pragma once

#include <cstddef>
#include <cstdint>
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_err.h"
#include "sdmmc_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool format_if_mount_failed;
    int max_files;
    size_t allocation_unit_size;
    bool disk_status_check_enable;
} esp_vfs_fat_sdmmc_mount_config_t;

inline esp_err_t esp_vfs_fat_sdmmc_mount(const char* base_path,
                                         const sdmmc_host_t* host_config,
                                         const void* slot_config,
                                         const esp_vfs_fat_sdmmc_mount_config_t* mount_config,
                                         sdmmc_card_t** out_card) {
    if (base_path == nullptr || host_config == nullptr || slot_config == nullptr ||
        mount_config == nullptr || out_card == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    static sdmmc_card_t dummy_card{0};
    *out_card = &dummy_card;
    return ESP_OK;
}

inline esp_err_t esp_vfs_fat_sdspi_mount(const char* base_path,
                                         const sdmmc_host_t* host_config,
                                         const sdspi_device_config_t* slot_config,
                                         const esp_vfs_fat_sdmmc_mount_config_t* mount_config,
                                         sdmmc_card_t** out_card) {
    if (base_path == nullptr || host_config == nullptr || slot_config == nullptr ||
        mount_config == nullptr || out_card == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    static sdmmc_card_t dummy_card{0};
    *out_card = &dummy_card;
    return ESP_OK;
}

inline esp_err_t esp_vfs_fat_sdcard_unmount(const char* base_path, sdmmc_card_t* card) {
    if (base_path == nullptr || card == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

inline esp_err_t esp_vfs_fat_info(const char* base_path,
                                  uint64_t* out_total_bytes,
                                  uint64_t* out_free_bytes) {
    if (base_path == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (out_total_bytes != nullptr) {
        *out_total_bytes = 64ULL * 1024ULL * 1024ULL * 1024ULL;
    }
    if (out_free_bytes != nullptr) {
        *out_free_bytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;
    }
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif
