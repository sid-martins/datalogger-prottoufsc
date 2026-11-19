#pragma once

#include "esp_adc/adc_oneshot.h"
#include "i_adc_hal.hpp"

namespace datalogger::lld {

/**
 * @brief LLD (Low-Level Driver / MCAL) do periférico ADC1 do ESP32-S3 (driver `esp_adc/adc_oneshot.h`).
 *
 * Configura e lê os 10 canais analógicos locais mapeados no BSP (`GPIO 1` a `GPIO 10`, `ADC1_CH0` a `ADC1_CH9`)
 * com resolução de 12 bits e atenuação de 12 dB.
 */
class AdcOneshotLld : public hal::IAdcHal {
public:
    static constexpr int kMaxVoltageMv = 3300;
    static constexpr int kMaxRaw12Bit  = 4095;

    AdcOneshotLld() = default;
    ~AdcOneshotLld() override;

    bool init() override;
    bool read_raw(hal::AdcChannelId channel, int& out_raw) override;
    bool read_millivolts(hal::AdcChannelId channel, int& out_mv) override;

private:
    adc_oneshot_unit_handle_t unit_handle_{nullptr};
};

} // namespace datalogger::lld
