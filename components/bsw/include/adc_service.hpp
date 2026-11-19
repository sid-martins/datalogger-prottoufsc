#pragma once

#include <array>
#include <cstdint>
#include "i_adc_hal.hpp"

namespace datalogger::bsw {

/**
 * @brief Parâmetros de calibração linear por canal (y = slope * mV + offset).
 */
struct AdcChannelCalibration {
    float slope{0.001f};
    float offset{0.0f};
};

/**
 * @brief Estado da última leitura de um canal analógico do ADC1.
 */
struct AdcChannelReading {
    int32_t raw_mv{0};
    float filtered_mv{0.0f};
    float physical_value{0.0f};
    uint64_t timestamp_ms{0};
    bool valid{false};
};

/**
 * @brief Snapshot consolidado dos 10 canais analógicos locais da placa central.
 */
struct AdcSnapshot {
    std::array<AdcChannelReading, hal::kAdcChannelCount> channels{};
    uint64_t last_update_ms{0};
};

/**
 * @brief BSW (Basic Software Service) - Serviço de Aquisição Analógica Local (ADC1 - 10 Canais).
 *
 * Responsável por amostrar os 10 canais do ADC1 definidos no BSP (`bsp::kAnalogChannels`),
 * validar a faixa elétrica (0..3300 mV), aplicar filtro digital passa-baixa IIR/EMA sem
 * alocação dinâmica de heap, suportar taxas de amostragem configuráveis por divisor de tick
 * e converter milivolts em unidades físicas de engenharia via calibração linear.
 */
class AdcService {
public:
    static constexpr int kMinValidMv = 0;
    static constexpr int kMaxValidMv = 3300;
    static constexpr float kDefaultFilterAlpha = 0.25f;

    explicit AdcService(hal::IAdcHal& adc_hal);

    /**
     * @brief Inicializa o driver ADC1 e reinicia os estados de amostragem/filtro.
     */
    bool initialize();

    /**
     * @brief Configura a calibração linear (physical = slope * filtered_mv + offset) de um canal.
     */
    bool set_channel_calibration(hal::AdcChannelId channel, float slope, float offset);

    /**
     * @brief Configura o fator alpha do filtro passa-baixa EMA no intervalo (0.0, 1.0].
     */
    bool set_channel_filter_alpha(hal::AdcChannelId channel, float alpha);

    /**
     * @brief Configura o divisor de taxa de amostragem por canal para `sample_tick()` (0 -> 1).
     */
    bool set_channel_sample_divider(hal::AdcChannelId channel, uint16_t divider);

    /**
     * @brief Executa a leitura, filtragem e conversão de um único canal analógico.
     */
    bool sample_channel(hal::AdcChannelId channel, uint64_t timestamp_ms);

    /**
     * @brief Executa a leitura de todos os 10 canais analógicos em sequência.
     */
    bool sample_all(uint64_t timestamp_ms);

    /**
     * @brief Executa um passo multi-taxa amostrando apenas os canais cujo divisor coincide com o tick atual.
     */
    bool sample_tick(uint64_t timestamp_ms);

    /**
     * @brief Reinicia o snapshot de leituras, estado dos filtros e contadores de amostragem.
     */
    void reset();

    bool is_initialized() const { return initialized_; }
    const AdcSnapshot& snapshot() const { return snapshot_; }
    const AdcChannelReading& channel_reading(hal::AdcChannelId channel) const;
    uint32_t sample_count() const { return sample_count_; }
    uint32_t read_error_count() const { return read_error_count_; }
    uint32_t tick_count() const { return tick_count_; }

private:
    hal::IAdcHal& adc_hal_;
    bool initialized_{false};
    AdcSnapshot snapshot_{};
    std::array<AdcChannelCalibration, hal::kAdcChannelCount> calib_{};
    std::array<float, hal::kAdcChannelCount> alpha_{};
    std::array<uint16_t, hal::kAdcChannelCount> divider_{};
    std::array<bool, hal::kAdcChannelCount> filter_initialized_{};
    uint32_t sample_count_{0};
    uint32_t read_error_count_{0};
    uint32_t tick_count_{0};
};

} // namespace datalogger::bsw
