#pragma once

#include "i_gpio_hal.hpp"

namespace datalogger::lld {

/**
 * @brief LLD (Low-Level Driver / MCAL) de GPIOs digitais da interface de pista no ESP32-S3.
 *
 * Configura `Free_SW1` (GPIO 35) e `Free_SW2` (GPIO 36) como entradas com pull-up habilitado
 * e `LED_Acquiring` (GPIO 37) / `LED_Working` (GPIO 39) como saídas push-pull iniciadas em nível baixo.
 */
class GpioLld : public hal::IGpioHal {
public:
    GpioLld() = default;
    ~GpioLld() override = default;

    GpioLld(const GpioLld&) = delete;
    GpioLld& operator=(const GpioLld&) = delete;

    /**
     * @brief Valida os pinos e aplica a configuração física via `driver/gpio.h`.
     */
    bool init(const hal::UiGpioConfig& config = hal::UiGpioConfig{}) override;

    /**
     * @brief Lê o nível lógico de um pino configurado da UI (`true` = HIGH, `false` = LOW).
     */
    bool read_pin(gpio_num_t pin, bool& out_high) const override;

    /**
     * @brief Escreve o nível lógico apenas nos pinos configurados como saída (LEDs).
     */
    bool write_pin(gpio_num_t pin, bool high) override;

    bool is_initialized() const { return initialized_; }

    /**
     * @brief Verifica se os 4 GPIOs da configuração são válidos no ESP32-S3 e distintos entre si.
     */
    static bool is_valid_config(const hal::UiGpioConfig& config);

private:
    static bool is_valid_gpio_pin(gpio_num_t pin);

    hal::UiGpioConfig config_{};
    bool initialized_{false};
};

} // namespace datalogger::lld
