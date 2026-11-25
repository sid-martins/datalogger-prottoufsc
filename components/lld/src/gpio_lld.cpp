#include "gpio_lld.hpp"

#include <array>
#include <cstddef>
#include "driver/gpio.h"
#include "esp_log.h"

namespace datalogger::lld {

namespace {
constexpr const char* kTag = "GPIO_LLD";
} // namespace

bool GpioLld::is_valid_gpio_pin(gpio_num_t pin) {
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

bool GpioLld::is_valid_config(const hal::UiGpioConfig& config) {
    const std::array<gpio_num_t, 4> pins{
        config.sw1_pin,
        config.sw2_beacon_pin,
        config.led_acquiring_pin,
        config.led_working_pin
    };

    for (std::size_t i = 0u; i < pins.size(); ++i) {
        if (!is_valid_gpio_pin(pins[i])) {
            return false;
        }
        for (std::size_t j = i + 1u; j < pins.size(); ++j) {
            if (pins[i] == pins[j]) {
                return false;
            }
        }
    }
    return true;
}

bool GpioLld::init(const hal::UiGpioConfig& config) {
    initialized_ = false;

    if (!is_valid_config(config)) {
        ESP_LOGE(kTag, "Configuracao de pinos GPIO da UI invalida");
        return false;
    }

    gpio_config_t in_cfg{};
    in_cfg.pin_bit_mask = (1ULL << static_cast<unsigned>(config.sw1_pin)) |
                          (1ULL << static_cast<unsigned>(config.sw2_beacon_pin));
    in_cfg.mode = GPIO_MODE_INPUT;
    in_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    in_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    in_cfg.intr_type = GPIO_INTR_DISABLE;

    if (gpio_config(&in_cfg) != ESP_OK) {
        ESP_LOGE(kTag, "Falha ao configurar botoes UI (SW1=%d, SW2=%d)",
                 static_cast<int>(config.sw1_pin),
                 static_cast<int>(config.sw2_beacon_pin));
        return false;
    }

    gpio_set_level(config.led_acquiring_pin, 0u);
    gpio_set_level(config.led_working_pin, 0u);

    gpio_config_t out_cfg{};
    out_cfg.pin_bit_mask = (1ULL << static_cast<unsigned>(config.led_acquiring_pin)) |
                           (1ULL << static_cast<unsigned>(config.led_working_pin));
    out_cfg.mode = GPIO_MODE_OUTPUT;
    out_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    out_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    out_cfg.intr_type = GPIO_INTR_DISABLE;

    if (gpio_config(&out_cfg) != ESP_OK) {
        ESP_LOGE(kTag, "Falha ao configurar LEDs UI (Acq=%d, Work=%d)",
                 static_cast<int>(config.led_acquiring_pin),
                 static_cast<int>(config.led_working_pin));
        return false;
    }

    gpio_set_level(config.led_acquiring_pin, 0u);
    gpio_set_level(config.led_working_pin, 0u);

    config_ = config;
    initialized_ = true;
    ESP_LOGI(kTag, "GPIO UI inicializado (SW1=%d [standby], SW2=%d [beacon], LED_Acq=%d, LED_Work=%d)",
             static_cast<int>(config_.sw1_pin),
             static_cast<int>(config_.sw2_beacon_pin),
             static_cast<int>(config_.led_acquiring_pin),
             static_cast<int>(config_.led_working_pin));
    return true;
}

bool GpioLld::read_pin(gpio_num_t pin, bool& out_high) const {
    if (!initialized_ || !is_valid_gpio_pin(pin)) {
        return false;
    }
    if (pin != config_.sw1_pin &&
        pin != config_.sw2_beacon_pin &&
        pin != config_.led_acquiring_pin &&
        pin != config_.led_working_pin) {
        return false;
    }
    out_high = (gpio_get_level(pin) != 0);
    return true;
}

bool GpioLld::write_pin(gpio_num_t pin, bool high) {
    if (!initialized_) {
        return false;
    }
    if (pin != config_.led_acquiring_pin && pin != config_.led_working_pin) {
        return false;
    }
    return gpio_set_level(pin, high ? 1u : 0u) == ESP_OK;
}

} // namespace datalogger::lld
