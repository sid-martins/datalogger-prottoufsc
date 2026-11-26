#pragma once

#include <cstdint>
#include "i_nvm_hal.hpp"
#include "nvs.h"

namespace datalogger::lld {

/**
 * @brief LLD (Low-Level Driver) para a partição NVS (Non-Volatile Storage) do ESP32-S3.
 *
 * Gerencia inicialização da partição Flash com wear-leveling nativo do ESP-IDF
 * (`nvs_flash`), auto-recuperação quando páginas de NVS requerem reformatação,
 * abertura de namespace e escrita/leitura atômica de inteiros `uint32_t`.
 */
class NvsLld : public hal::INvmHal {
public:
    NvsLld() = default;
    ~NvsLld() override;

    NvsLld(const NvsLld&) = delete;
    NvsLld& operator=(const NvsLld&) = delete;

    /**
     * @brief Valida se os nomes de partição e namespace atendem ao limite da NVS (`1..15` chars).
     */
    static bool is_valid_config(const hal::NvmConfig& config);

    bool init(const hal::NvmConfig& config) override;
    hal::NvmReadResult read_u32(const char* key, uint32_t& out_value) const override;
    bool write_u32(const char* key, uint32_t value) override;
    bool commit() override;

    bool is_open() const { return opened_; }

private:
    static bool is_valid_nvs_name(const char* name);

    nvs_handle_t handle_{0u};
    bool opened_{false};
};

} // namespace datalogger::lld
