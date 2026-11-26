#include "nvs_lld.hpp"

#include <cstring>
#include "esp_log.h"
#include "nvs_flash.h"

namespace datalogger::lld {

namespace {
constexpr const char* kTag = "NVS_LLD";
} // namespace

NvsLld::~NvsLld() {
    if (opened_) {
        nvs_close(handle_);
        handle_ = 0u;
        opened_ = false;
    }
}

bool NvsLld::is_valid_nvs_name(const char* name) {
    if (name == nullptr) {
        return false;
    }
    const std::size_t len = std::strlen(name);
    return (len >= 1u) && (len < static_cast<std::size_t>(NVS_KEY_NAME_MAX_SIZE));
}

bool NvsLld::is_valid_config(const hal::NvmConfig& config) {
    return is_valid_nvs_name(config.partition_name) &&
           is_valid_nvs_name(config.namespace_name);
}

bool NvsLld::init(const hal::NvmConfig& config) {
    if (opened_) {
        nvs_close(handle_);
        handle_ = 0u;
        opened_ = false;
    }

    if (!is_valid_config(config)) {
        ESP_LOGE(kTag, "Configuracao de particao/namespace NVS invalida");
        return false;
    }

    esp_err_t err = nvs_flash_init_partition(config.partition_name);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(kTag, "Particao NVS '%s' sem paginas livres/nova versao; reformatando...",
                 config.partition_name);
        if (nvs_flash_erase_partition(config.partition_name) == ESP_OK) {
            err = nvs_flash_init_partition(config.partition_name);
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Falha ao inicializar particao NVS '%s' (err=%d)",
                 config.partition_name, static_cast<int>(err));
        return false;
    }

    nvs_handle_t new_handle = 0u;
    err = nvs_open_from_partition(config.partition_name,
                                  config.namespace_name,
                                  NVS_READWRITE,
                                  &new_handle);
    if (err != ESP_OK || new_handle == 0u) {
        ESP_LOGE(kTag, "Falha ao abrir namespace NVS '%s' (err=%d)",
                 config.namespace_name, static_cast<int>(err));
        return false;
    }

    handle_ = new_handle;
    opened_ = true;
    return true;
}

hal::NvmReadResult NvsLld::read_u32(const char* key, uint32_t& out_value) const {
    if (!opened_ || !is_valid_nvs_name(key)) {
        return hal::NvmReadResult::kError;
    }

    const esp_err_t err = nvs_get_u32(handle_, key, &out_value);
    if (err == ESP_OK) {
        return hal::NvmReadResult::kOk;
    }
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return hal::NvmReadResult::kNotFound;
    }
    return hal::NvmReadResult::kError;
}

bool NvsLld::write_u32(const char* key, uint32_t value) {
    if (!opened_ || !is_valid_nvs_name(key)) {
        return false;
    }
    return nvs_set_u32(handle_, key, value) == ESP_OK;
}

bool NvsLld::commit() {
    if (!opened_) {
        return false;
    }
    return nvs_commit(handle_) == ESP_OK;
}

} // namespace datalogger::lld
