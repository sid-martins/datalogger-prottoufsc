#pragma once

#include <cstddef>
#include <cstdint>
#include "bsp_pins.h"

namespace datalogger::hal {

/**
 * @brief Configuração da interface UART para o módulo GPS u-blox NEO-8M.
 */
struct GpsConfig {
    gpio_num_t rx_pin{PIN_GPS_RX};        // GPIO 13 (ESP32-S3 RX <- NEO-8M TX)
    gpio_num_t tx_pin{PIN_GPS_TX};        // GPIO 47 (ESP32-S3 TX -> NEO-8M RX)
    uint32_t initial_baud_rate{9600};     // Baud padrão de fábrica do NEO-8M
    uint32_t operating_baud_rate{115200}; // Baud operacional para 10 Hz sem saturação
    uint8_t update_rate_hz{10};           // Taxa de navegação alvo (10 Hz = 100 ms)
    uint8_t dynamic_model{4};             // UBX-CFG-NAV5 dynModel: 4 = Automotive
    uint8_t uart_port{1};                 // UART_NUM_1 no ESP32-S3
};

/**
 * @brief Snapshot consolidado de posição, cinemática, qualidade de fix e tempo do GPS.
 */
struct GpsData {
    double latitude_deg{0.0};
    double longitude_deg{0.0};
    float altitude_m{0.0f};
    float speed_knots{0.0f};
    float speed_kmh{0.0f};
    float speed_mps{0.0f};
    float course_deg{0.0f};
    float hdop{99.99f};
    uint8_t satellites{0};
    uint8_t fix_quality{0};
    bool has_fix{false};
    bool gga_valid{false};
    bool rmc_valid{false};
    uint32_t utc_time_ms{0};
    uint32_t utc_date_yyyymmdd{0};
    uint64_t timestamp_ms{0};
    uint64_t monotonic_ms{0};
    bool time_synchronized{false};
};

/**
 * @brief Interface HAL (Hardware Abstraction Layer) para transporte UART do GPS NEO-8M.
 */
class IGpsHal {
public:
    virtual ~IGpsHal() = default;

    /**
     * @brief Inicializa o periférico UART e configura o módulo GPS (ex.: 115200 baud, 10 Hz).
     */
    virtual bool init(const GpsConfig& config) = 0;

    /**
     * @brief Lê bytes recebidos da UART para o buffer informado.
     * @return Quantidade de bytes lidos (> 0), 0 em timeout/sem dados, ou < 0 em erro.
     */
    virtual int read_bytes(uint8_t* buffer, std::size_t max_len, uint32_t timeout_ms) = 0;

    /**
     * @brief Envia um buffer de bytes pela UART para o módulo GPS.
     */
    virtual bool write_bytes(const uint8_t* data, std::size_t len) = 0;
};

} // namespace datalogger::hal
