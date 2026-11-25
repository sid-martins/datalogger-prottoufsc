#pragma once

#include <cstddef>
#include <cstdint>
#include "bsp_pins.h"

namespace datalogger::hal {

inline constexpr std::size_t kAdcChannelCount = 10u;

/**
 * @brief Identificadores dos 10 canais analógicos locais mapeados no ADC1 (GPIO 1..10).
 */
enum class AdcChannelId : uint8_t {
    kOilPressure  = 0, // GPIO_NUM_1  (ADC1_CH0)
    kOilTemp      = 1, // GPIO_NUM_2  (ADC1_CH1)
    kFuelPressure = 2, // GPIO_NUM_3  (ADC1_CH2)
    kLambda       = 3, // GPIO_NUM_4  (ADC1_CH3)
    kAnalog5V1    = 4, // GPIO_NUM_5  (ADC1_CH4)
    kAnalog5V2    = 5, // GPIO_NUM_6  (ADC1_CH5)
    kAnalog5V3    = 6, // GPIO_NUM_7  (ADC1_CH6)
    kAnalog5V4    = 7, // GPIO_NUM_8  (ADC1_CH7)
    kAnalog12V1   = 8, // GPIO_NUM_9  (ADC1_CH8)
    kAnalog12V2   = 9, // GPIO_NUM_10 (ADC1_CH9)
    kCount        = 10
};

/**
 * @brief Interface HAL (Hardware Abstraction Layer) para leitura dos canais do ADC1.
 */
class IAdcHal {
public:
    virtual ~IAdcHal() = default;

    /**
     * @brief Inicializa o periférico ADC1 e configura os 10 canais analógicos.
     */
    virtual bool init() = 0;

    /**
     * @brief Lê o valor bruto (12-bit: 0..4095) do canal especificado.
     */
    virtual bool read_raw(AdcChannelId channel, int& out_raw) = 0;

    /**
     * @brief Lê a tensão em milivolts (0..3300 mV) do canal especificado.
     */
    virtual bool read_millivolts(AdcChannelId channel, int& out_mv) = 0;
};

} // namespace datalogger::hal
