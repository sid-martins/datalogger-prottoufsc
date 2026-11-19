#include "adc_oneshot_lld.hpp"

#include <cstddef>
#include "bsp_pins.h"
#include "esp_log.h"

namespace datalogger::lld {

namespace {

constexpr const char* kTag = "ADC_ONESHOT_LLD";

bool gpio_to_adc1_channel(gpio_num_t pin, adc_channel_t& out_channel) {
    switch (pin) {
        case PIN_ADC_OIL_PRESSURE:
            out_channel = ADC_CHANNEL_0;
            return true;
        case PIN_ADC_OIL_TEMP:
            out_channel = ADC_CHANNEL_1;
            return true;
        case PIN_ADC_FUEL_PRESSURE:
            out_channel = ADC_CHANNEL_2;
            return true;
        case PIN_ADC_LAMBDA:
            out_channel = ADC_CHANNEL_3;
            return true;
        case PIN_ADC_5TO3V3_1:
            out_channel = ADC_CHANNEL_4;
            return true;
        case PIN_ADC_5TO3V3_2:
            out_channel = ADC_CHANNEL_5;
            return true;
        case PIN_ADC_5TO3V3_3:
            out_channel = ADC_CHANNEL_6;
            return true;
        case PIN_ADC_5TO3V3_4:
            out_channel = ADC_CHANNEL_7;
            return true;
        case PIN_ADC_12TO3V3_1:
            out_channel = ADC_CHANNEL_8;
            return true;
        case PIN_ADC_12TO3V3_2:
            out_channel = ADC_CHANNEL_9;
            return true;
        default:
            return false;
    }
}

} // namespace

AdcOneshotLld::~AdcOneshotLld() {
    if (unit_handle_ != nullptr) {
        adc_oneshot_del_unit(unit_handle_);
        unit_handle_ = nullptr;
    }
}

bool AdcOneshotLld::init() {
    if (unit_handle_ != nullptr) {
        adc_oneshot_del_unit(unit_handle_);
        unit_handle_ = nullptr;
    }

    adc_oneshot_unit_init_cfg_t init_cfg{};
    init_cfg.unit_id = ADC_UNIT_1;
    init_cfg.ulp_mode = ADC_ULP_MODE_DISABLE;

    if (adc_oneshot_new_unit(&init_cfg, &unit_handle_) != ESP_OK || unit_handle_ == nullptr) {
        ESP_LOGE(kTag, "Falha ao inicializar unidade ADC1");
        unit_handle_ = nullptr;
        return false;
    }

    adc_oneshot_chan_cfg_t chan_cfg{};
    chan_cfg.atten = ADC_ATTEN_DB_12;
    chan_cfg.bitwidth = ADC_BITWIDTH_12;

    for (std::size_t i = 0; i < hal::kAdcChannelCount; ++i) {
        adc_channel_t adc_ch = ADC_CHANNEL_0;
        if (!gpio_to_adc1_channel(bsp::kAnalogChannels[i].pin, adc_ch)) {
            ESP_LOGE(kTag, "Pino invalido para ADC1 no canal %s", bsp::kAnalogChannels[i].tag);
            adc_oneshot_del_unit(unit_handle_);
            unit_handle_ = nullptr;
            return false;
        }
        if (adc_oneshot_config_channel(unit_handle_, adc_ch, &chan_cfg) != ESP_OK) {
            ESP_LOGE(kTag, "Falha ao configurar canal %s (GPIO %d)",
                     bsp::kAnalogChannels[i].tag,
                     static_cast<int>(bsp::kAnalogChannels[i].pin));
            adc_oneshot_del_unit(unit_handle_);
            unit_handle_ = nullptr;
            return false;
        }
    }

    ESP_LOGI(kTag, "ADC1 configurado com sucesso para %u canais locais (12-bit, DB_12)",
             static_cast<unsigned>(hal::kAdcChannelCount));
    return true;
}

bool AdcOneshotLld::read_raw(hal::AdcChannelId channel, int& out_raw) {
    if (unit_handle_ == nullptr) {
        return false;
    }
    const auto idx = static_cast<std::size_t>(channel);
    if (idx >= hal::kAdcChannelCount) {
        return false;
    }

    adc_channel_t adc_ch = ADC_CHANNEL_0;
    if (!gpio_to_adc1_channel(bsp::kAnalogChannels[idx].pin, adc_ch)) {
        return false;
    }

    return adc_oneshot_read(unit_handle_, adc_ch, &out_raw) == ESP_OK;
}

bool AdcOneshotLld::read_millivolts(hal::AdcChannelId channel, int& out_mv) {
    int raw = 0;
    if (!read_raw(channel, raw)) {
        return false;
    }
    if (raw < 0) {
        raw = 0;
    } else if (raw > kMaxRaw12Bit) {
        raw = kMaxRaw12Bit;
    }
    out_mv = (raw * kMaxVoltageMv) / kMaxRaw12Bit;
    return true;
}

} // namespace datalogger::lld
