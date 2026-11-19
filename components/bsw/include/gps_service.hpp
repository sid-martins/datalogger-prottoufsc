#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include "i_gps_hal.hpp"
#include "rtc_service.hpp"

namespace datalogger::bsw {

/**
 * @brief BSW (Basic Software Service) - Serviço determinístico de aquisição e parsing GPS NMEA 0183.
 *
 * Opera com zero alocação dinâmica de heap utilizando acumulador fixo de linha (`128 bytes`),
 * filtro limitado para frames binários UBX (`0xB5 0x62`), validação de checksum XOR NMEA,
 * decodificação manual de sentenças `$xxGGA` e `$xxRMC` (12 a 14 campos) e carimbo temporal
 * unificado via `RtcService::sample_timestamp()` (com fallback monotônico de 1 ms).
 */
class GpsService {
public:
    static constexpr std::size_t kLineBufferSize = 128u;
    static constexpr std::size_t kRxChunkSize = 256u;
    static constexpr std::size_t kMaxNmeaFields = 16u;
    static constexpr std::size_t kMaxUbxSkipLen = 256u;
    static constexpr float kKnotsToKmh = 1.852f;
    static constexpr float kKnotsToMps = 1852.0f / 3600.0f;

    explicit GpsService(hal::IGpsHal& gps_hal, const RtcService& rtc_service);

    /**
     * @brief Inicializa o HAL do GPS e zera estado e contadores de diagnóstico.
     */
    bool initialize(const hal::GpsConfig& config = hal::GpsConfig{});

    /**
     * @brief Lê um bloco da UART via HAL e processa os bytes recebidos.
     * @return true se ao menos uma sentença GGA ou RMC válida foi decodificada nesta chamada.
     */
    bool poll(uint32_t timeout_ms = 0);

    /**
     * @brief Valida checksum e decodifica uma sentença NMEA completa (`$xxGGA` ou `$xxRMC`).
     */
    bool parse_nmea_sentence(std::string_view sentence);

    /**
     * @brief Valida o formato `$...*HH` e o checksum XOR de 8 bits da sentença NMEA.
     */
    static bool validate_nmea_checksum(std::string_view sentence);

    const hal::GpsData& latest_data() const { return latest_data_; }
    bool is_initialized() const { return initialized_; }
    uint32_t sentences_parsed() const { return sentences_parsed_; }
    uint32_t checksum_errors() const { return checksum_errors_; }
    uint32_t rx_bytes() const { return rx_bytes_; }
    uint32_t overflow_count() const { return overflow_count_; }

private:
    enum class FrameState : uint8_t {
        kIdle = 0,
        kNmeaBody,
        kUbxSync2,
        kUbxClass,
        kUbxId,
        kUbxLenL,
        kUbxLenH,
        kUbxSkip
    };

    bool process_byte(uint8_t byte);
    void reset_parser_state();
    bool parse_gga(const std::array<std::string_view, kMaxNmeaFields>& fields, std::size_t field_count);
    bool parse_rmc(const std::array<std::string_view, kMaxNmeaFields>& fields, std::size_t field_count);
    void stamp_rtc_time();

    hal::IGpsHal& gps_hal_;
    const RtcService& rtc_service_;
    bool initialized_{false};
    hal::GpsData latest_data_{};
    std::array<char, kLineBufferSize> line_buf_{};
    std::size_t line_len_{0};
    bool line_overflowed_{false};
    FrameState frame_state_{FrameState::kIdle};
    uint8_t ubx_len_lsb_{0};
    std::size_t ubx_remaining_{0};
    uint32_t sentences_parsed_{0};
    uint32_t checksum_errors_{0};
    uint32_t rx_bytes_{0};
    uint32_t overflow_count_{0};
};

} // namespace datalogger::bsw
