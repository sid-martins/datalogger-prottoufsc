#pragma once

#include <cstdint>
#include "bsp_pins.h"

namespace datalogger::hal {

/**
 * @brief Configuração dos pinos digitais da interface de pista (botões e LEDs no BSP).
 */
struct UiGpioConfig {
    gpio_num_t sw1_pin{PIN_BTN_FREE_SW1};            // GPIO 35 (Em standby / reservado, ativo baixo)
    gpio_num_t sw2_beacon_pin{PIN_BTN_FREE_SW2};     // GPIO 36 (Lap Beacon, ativo baixo)
    gpio_num_t led_acquiring_pin{PIN_LED_ACQUIRING}; // GPIO 37 (LED Acquiring, ativo alto)
    gpio_num_t led_working_pin{PIN_LED_WORKING};     // GPIO 39 (LED Working, ativo alto)
};

/**
 * @brief Interface HAL (Hardware Abstraction Layer) para leitura de botões e acionamento de LEDs.
 */
class IGpioHal {
public:
    virtual ~IGpioHal() = default;

    /**
     * @brief Inicializa os pinos de entrada (botões) e saída (LEDs) da interface de pista.
     */
    virtual bool init(const UiGpioConfig& config = UiGpioConfig{}) = 0;

    /**
     * @brief Lê o nível lógico elétrico de um pino digital (true = HIGH, false = LOW).
     */
    virtual bool read_pin(gpio_num_t pin, bool& out_high) const = 0;

    /**
     * @brief Escreve o nível lógico elétrico em um pino digital de saída (true = HIGH, false = LOW).
     */
    virtual bool write_pin(gpio_num_t pin, bool high) = 0;
};

} // namespace datalogger::hal
