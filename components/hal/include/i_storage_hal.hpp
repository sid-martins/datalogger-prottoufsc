#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include "bsp_pins.h"

namespace datalogger::hal {

/**
 * @brief Modo físico de barramento do cartão MicroSD (conector J4).
 */
enum class SdBusMode : uint8_t {
    kSpi       = 0,
    kSdmmc4Bit = 1
};

/**
 * @brief Configuração de montagem do sistema de arquivos FAT no cartão MicroSD.
 */
struct StorageConfig {
    SdBusMode bus_mode{SdBusMode::kSdmmc4Bit};
    gpio_num_t clk_pin{PIN_SD_CLK};   // GPIO 21
    gpio_num_t cmd_pin{PIN_SD_CMD};   // GPIO 18 (MOSI)
    gpio_num_t d0_pin{PIN_SD_DAT0};   // GPIO 14 (MISO)
    gpio_num_t d1_pin{PIN_SD_DAT1};   // GPIO 40
    gpio_num_t d2_pin{PIN_SD_DAT2};   // GPIO 41
    gpio_num_t d3_pin{PIN_SD_DAT3};   // GPIO 17 (CS)
    const char* mount_point{"/sdcard"};
    uint32_t max_freq_khz{20000u};
    bool format_if_mount_failed{false};
};

/**
 * @brief HAL (Hardware Abstraction Layer) para armazenamento não-volátil (MicroSD J4 / Flash).
 */
class IStorageHal {
public:
    virtual ~IStorageHal() = default;

    /**
     * @brief Monta o sistema de arquivos da mídia de armazenamento com a configuração dada.
     */
    virtual bool mount(const StorageConfig& config) = 0;

    /**
     * @brief Desmonta a mídia de armazenamento de forma segura.
     */
    virtual bool unmount() = 0;

    /**
     * @brief Indica se o sistema de arquivos está montado e pronto para I/O.
     */
    virtual bool is_mounted() const = 0;

    /**
     * @brief Abre ou cria um arquivo para escrita sequencial em modo append binário.
     */
    virtual bool open_file(std::string_view file_path) = 0;

    /**
     * @brief Grava um bloco de bytes no arquivo atualmente aberto.
     */
    virtual bool write(const char* data, std::size_t len) = 0;

    /**
     * @brief Força a descarga dos buffers do arquivo para a mídia física (fflush + fsync).
     */
    virtual bool sync() = 0;

    /**
     * @brief Sincroniza e fecha o arquivo atualmente aberto.
     */
    virtual bool close_file() = 0;

    /**
     * @brief Consulta o espaço livre disponível no volume montado em bytes.
     */
    virtual bool get_free_space_bytes(uint64_t& out_free_bytes) const = 0;
};

} // namespace datalogger::hal
