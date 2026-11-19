#pragma once

#include <cstdint>
#include "adc_service.hpp"
#include "can_decoder_service.hpp"
#include "gps_service.hpp"
#include "i_osal_queue.hpp"
#include "rtc_service.hpp"
#include "telemetry_snapshot.hpp"

namespace datalogger::bsw {

/**
 * @brief BSW (Basic Software Service) - Agregador de telemetria multi-domínio (Core 1 -> Core 0).
 *
 * Captura de forma determinística e sem alocação dinâmica um `TelemetrySnapshot` completo
 * unificando RTC, CAN (6 nós ESP32-C3 + ECU), ADC1 (10 canais) e GPS, publicando-o
 * em uma fila OSAL para consumo e gravação no MicroSD pelo Core 0.
 */
class TelemetryAggregator {
public:
    TelemetryAggregator(const RtcService& rtc_service,
                        const CanDecoderService& can_decoder,
                        const AdcService& adc_service,
                        const GpsService& gps_service);

    /**
     * @brief Captura um snapshot unificado do estado atual de todos os subsistemas.
     */
    TelemetrySnapshot capture_snapshot(uint16_t lap_beacon = 0u,
                                       uint64_t can_timeout_ms = CanDecoderService::kDefaultNodeTimeoutMs);

    /**
     * @brief Captura e envia um snapshot para a fila OSAL de destino.
     */
    bool publish_snapshot(osal::IOsalQueue<TelemetrySnapshot>& target_queue,
                          uint32_t queue_timeout_ms = 0u,
                          uint16_t lap_beacon = 0u,
                          uint64_t can_timeout_ms = CanDecoderService::kDefaultNodeTimeoutMs);

    /**
     * @brief Reinicia os contadores de sequência, publicação e descarte.
     */
    void reset();

    uint32_t sample_seq() const { return sample_seq_; }
    uint32_t published_count() const { return published_count_; }
    uint32_t dropped_count() const { return dropped_count_; }

private:
    const RtcService& rtc_service_;
    const CanDecoderService& can_decoder_;
    const AdcService& adc_service_;
    const GpsService& gps_service_;
    uint32_t sample_seq_{0};
    uint32_t published_count_{0};
    uint32_t dropped_count_{0};
};

} // namespace datalogger::bsw
