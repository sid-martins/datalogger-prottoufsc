#include "sd_card_lld.hpp"

#include <cstring>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"

namespace datalogger::lld {

namespace {

constexpr const char* kTag = "SD_CARD_LLD";

bool is_valid_gpio_pin(gpio_num_t pin) {
    const int p = static_cast<int>(pin);
    if (p < 0 || p >= static_cast<int>(GPIO_NUM_MAX)) {
        return false;
    }
    // GPIOs 26..32 são reservados à Flash SPI interna do ESP32-S3
    if (p >= 26 && p <= 32) {
        return false;
    }
    return true;
}

bool are_pins_distinct(const gpio_num_t* pins, std::size_t count) {
    for (std::size_t i = 0u; i < count; ++i) {
        if (!is_valid_gpio_pin(pins[i])) {
            return false;
        }
        for (std::size_t j = i + 1u; j < count; ++j) {
            if (pins[i] == pins[j]) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

SdCardLld::~SdCardLld() {
    if (mounted_) {
        unmount();
    }
}

bool SdCardLld::is_valid_config(const hal::StorageConfig& config) {
    if (config.mount_point == nullptr || config.mount_point[0] != '/') {
        return false;
    }
    if (config.max_freq_khz == 0u) {
        return false;
    }

    if (config.bus_mode == hal::SdBusMode::kSdmmc4Bit) {
        const std::array<gpio_num_t, 6> pins{
            config.clk_pin,
            config.cmd_pin,
            config.d0_pin,
            config.d1_pin,
            config.d2_pin,
            config.d3_pin
        };
        return are_pins_distinct(pins.data(), pins.size());
    }

    const std::array<gpio_num_t, 4> spi_pins{
        config.clk_pin,
        config.cmd_pin,
        config.d0_pin,
        config.d3_pin
    };
    return are_pins_distinct(spi_pins.data(), spi_pins.size());
}

bool SdCardLld::mount(const hal::StorageConfig& config) {
    if (mounted_) {
        unmount();
    }

    if (!is_valid_config(config)) {
        ESP_LOGE(kTag, "Configuracao de pinos/parametros MicroSD invalida");
        return false;
    }

    const std::size_t mp_len = std::strlen(config.mount_point);
    if (mp_len == 0u || mp_len >= mount_point_.size()) {
        ESP_LOGE(kTag, "Ponto de montagem invalido");
        return false;
    }
    std::memcpy(mount_point_.data(), config.mount_point, mp_len);
    mount_point_[mp_len] = '\0';

    config_ = config;
    config_.mount_point = mount_point_.data();

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg{};
    mount_cfg.format_if_mount_failed = config.format_if_mount_failed;
    mount_cfg.max_files = 4;
    mount_cfg.allocation_unit_size = 16u * 1024u;
    mount_cfg.disk_status_check_enable = false;

    if (config.bus_mode == hal::SdBusMode::kSdmmc4Bit) {
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.max_freq_khz = static_cast<int>(config.max_freq_khz);

        sdmmc_slot_config_t slot_cfg = SDMMC_SLOT_CONFIG_DEFAULT();
        slot_cfg.width = 4u;
        slot_cfg.clk = config.clk_pin;
        slot_cfg.cmd = config.cmd_pin;
        slot_cfg.d0 = config.d0_pin;
        slot_cfg.d1 = config.d1_pin;
        slot_cfg.d2 = config.d2_pin;
        slot_cfg.d3 = config.d3_pin;
        slot_cfg.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

        const esp_err_t err = esp_vfs_fat_sdmmc_mount(
            mount_point_.data(), &host, &slot_cfg, &mount_cfg, &card_);
        if (err != ESP_OK || card_ == nullptr) {
            ESP_LOGE(kTag, "Falha ao montar MicroSD em SDMMC 4-bit (err=%d)", static_cast<int>(err));
            card_ = nullptr;
            return false;
        }
    } else {
        spi_bus_config_t bus_cfg{};
        bus_cfg.mosi_io_num = static_cast<int>(config.cmd_pin);
        bus_cfg.miso_io_num = static_cast<int>(config.d0_pin);
        bus_cfg.sclk_io_num = static_cast<int>(config.clk_pin);
        bus_cfg.quadwp_io_num = -1;
        bus_cfg.quadhd_io_num = -1;
        bus_cfg.max_transfer_sz = 4096;

        spi_host_ = SPI2_HOST;
        if (spi_bus_initialize(spi_host_, &bus_cfg, SPI_DMA_CH_AUTO) != ESP_OK) {
            ESP_LOGE(kTag, "Falha ao inicializar barramento SPI para MicroSD");
            return false;
        }
        spi_bus_initialized_ = true;

        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.max_freq_khz = static_cast<int>(config.max_freq_khz);

        sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot_cfg.gpio_cs = config.d3_pin;
        slot_cfg.host_id = spi_host_;

        const esp_err_t err = esp_vfs_fat_sdspi_mount(
            mount_point_.data(), &host, &slot_cfg, &mount_cfg, &card_);
        if (err != ESP_OK || card_ == nullptr) {
            ESP_LOGE(kTag, "Falha ao montar MicroSD em SPI (err=%d)", static_cast<int>(err));
            spi_bus_free(spi_host_);
            spi_bus_initialized_ = false;
            card_ = nullptr;
            return false;
        }
    }

    mounted_ = true;
    ESP_LOGI(kTag, "MicroSD montado em %s (%s, %lu kHz)",
             mount_point_.data(),
             config.bus_mode == hal::SdBusMode::kSdmmc4Bit ? "SDMMC 4-bit" : "SPI",
             static_cast<unsigned long>(config.max_freq_khz));
    return true;
}

bool SdCardLld::unmount() {
    if (!mounted_) {
        return false;
    }

    if (file_ != nullptr) {
        close_file();
    }

    const esp_err_t err = esp_vfs_fat_sdcard_unmount(mount_point_.data(), card_);
    if (spi_bus_initialized_) {
        spi_bus_free(spi_host_);
        spi_bus_initialized_ = false;
    }

    card_ = nullptr;
    mounted_ = false;
    return err == ESP_OK;
}

bool SdCardLld::open_file(std::string_view file_path) {
    if (!mounted_ || file_ != nullptr || file_path.empty()) {
        return false;
    }

    std::array<char, 128> path_buf{};
    if (file_path.size() >= path_buf.size()) {
        return false;
    }

    std::memcpy(path_buf.data(), file_path.data(), file_path.size());
    path_buf[file_path.size()] = '\0';

    file_ = std::fopen(path_buf.data(), "ab");
    if (file_ == nullptr) {
        ESP_LOGE(kTag, "Falha ao abrir arquivo %s", path_buf.data());
        return false;
    }
    return true;
}

bool SdCardLld::write(const char* data, std::size_t len) {
    if (!mounted_ || file_ == nullptr) {
        return false;
    }
    if (len == 0u) {
        return true;
    }
    if (data == nullptr) {
        return false;
    }
    return std::fwrite(data, 1u, len, file_) == len;
}

bool SdCardLld::sync() {
    if (!mounted_ || file_ == nullptr) {
        return false;
    }
    if (std::fflush(file_) != 0) {
        return false;
    }
#if defined(_WIN32)
    const int fd = _fileno(file_);
    if (fd < 0 || _commit(fd) != 0) {
        return false;
    }
#else
    const int fd = fileno(file_);
    if (fd < 0 || fsync(fd) != 0) {
        return false;
    }
#endif
    return true;
}

bool SdCardLld::close_file() {
    if (file_ == nullptr) {
        return false;
    }
    const bool synced = sync();
    const int closed = std::fclose(file_);
    file_ = nullptr;
    return synced && (closed == 0);
}

bool SdCardLld::get_free_space_bytes(uint64_t& out_free_bytes) const {
    if (!mounted_) {
        return false;
    }
    uint64_t total_bytes = 0ULL;
    uint64_t free_bytes = 0ULL;
    if (esp_vfs_fat_info(mount_point_.data(), &total_bytes, &free_bytes) != ESP_OK) {
        return false;
    }
    out_free_bytes = free_bytes;
    return true;
}

} // namespace datalogger::lld
