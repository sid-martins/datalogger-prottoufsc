#pragma once

#include <array>
#include <cstdio>
#include <cstdint>
#include "driver/spi_common.h"
#include "i_storage_hal.hpp"
#include "sdmmc_cmd.h"

namespace datalogger::lld {

/**
 * @brief LLD (Low-Level Driver / MCAL) para cartão MicroSD no conector J4 do ESP32-S3.
 *
 * Suporta montagem FATFS via SDMMC 4-bit (GPIO Matrix: CLK=21, CMD=18, D0=14, D1=40, D2=41, D3=17)
 * ou SPI (SDSPI), além de escrita sequencial, descarga física (`fflush` + `fsync`/`_commit`)
 * e consulta de espaço livre do volume.
 */
class SdCardLld : public hal::IStorageHal {
public:
    SdCardLld() = default;
    ~SdCardLld() override;

    bool mount(const hal::StorageConfig& config) override;
    bool unmount() override;
    bool is_mounted() const override { return mounted_; }
    bool open_file(std::string_view file_path) override;
    bool write(const char* data, std::size_t len) override;
    bool sync() override;
    bool close_file() override;
    bool get_free_space_bytes(uint64_t& out_free_bytes) const override;

    static bool is_valid_config(const hal::StorageConfig& config);

private:
    hal::StorageConfig config_{};
    std::array<char, 32> mount_point_{};
    sdmmc_card_t* card_{nullptr};
    std::FILE* file_{nullptr};
    bool mounted_{false};
    bool spi_bus_initialized_{false};
    spi_host_device_t spi_host_{SPI2_HOST};
};

} // namespace datalogger::lld
