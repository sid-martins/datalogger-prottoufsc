#include "adc_service.hpp"

#include <cmath>
#include <cstddef>

namespace datalogger::bsw {

AdcService::AdcService(hal::IAdcHal& adc_hal)
    : adc_hal_(adc_hal) {
    alpha_.fill(kDefaultFilterAlpha);
    divider_.fill(1u);
}

bool AdcService::initialize() {
    reset();
    if (!adc_hal_.init()) {
        initialized_ = false;
        return false;
    }
    initialized_ = true;
    return true;
}

bool AdcService::set_channel_calibration(hal::AdcChannelId channel, float slope, float offset) {
    const auto idx = static_cast<std::size_t>(channel);
    if (idx >= hal::kAdcChannelCount) {
        return false;
    }
    calib_[idx].slope = slope;
    calib_[idx].offset = offset;
    return true;
}

bool AdcService::set_channel_filter_alpha(hal::AdcChannelId channel, float alpha) {
    const auto idx = static_cast<std::size_t>(channel);
    if (idx >= hal::kAdcChannelCount) {
        return false;
    }
    if (std::isnan(alpha) || alpha <= 0.0f || alpha > 1.0f) {
        return false;
    }
    alpha_[idx] = alpha;
    return true;
}

bool AdcService::set_channel_sample_divider(hal::AdcChannelId channel, uint16_t divider) {
    const auto idx = static_cast<std::size_t>(channel);
    if (idx >= hal::kAdcChannelCount) {
        return false;
    }
    divider_[idx] = (divider == 0u) ? 1u : divider;
    return true;
}

bool AdcService::sample_channel(hal::AdcChannelId channel, uint64_t timestamp_ms) {
    const auto idx = static_cast<std::size_t>(channel);
    if (idx >= hal::kAdcChannelCount) {
        ++read_error_count_;
        return false;
    }

    int mv = 0;
    if (!adc_hal_.read_millivolts(channel, mv)) {
        snapshot_.channels[idx].valid = false;
        ++read_error_count_;
        return false;
    }

    if (mv < kMinValidMv || mv > kMaxValidMv) {
        snapshot_.channels[idx].valid = false;
        ++read_error_count_;
        return false;
    }

    auto& ch = snapshot_.channels[idx];
    const float sample_mv = static_cast<float>(mv);
    if (!filter_initialized_[idx]) {
        ch.filtered_mv = sample_mv;
        filter_initialized_[idx] = true;
    } else {
        const float a = alpha_[idx];
        ch.filtered_mv = (a * sample_mv) + ((1.0f - a) * ch.filtered_mv);
    }

    ch.raw_mv = static_cast<int32_t>(mv);
    ch.physical_value = (calib_[idx].slope * ch.filtered_mv) + calib_[idx].offset;
    ch.timestamp_ms = timestamp_ms;
    ch.valid = true;
    snapshot_.last_update_ms = timestamp_ms;
    ++sample_count_;
    return true;
}

bool AdcService::sample_all(uint64_t timestamp_ms) {
    bool all_ok = true;
    for (std::size_t i = 0; i < hal::kAdcChannelCount; ++i) {
        const auto channel = static_cast<hal::AdcChannelId>(i);
        if (!sample_channel(channel, timestamp_ms)) {
            all_ok = false;
        }
    }
    return all_ok;
}

bool AdcService::sample_tick(uint64_t timestamp_ms) {
    bool all_ok = true;
    for (std::size_t i = 0; i < hal::kAdcChannelCount; ++i) {
        if ((tick_count_ % divider_[i]) == 0u) {
            const auto channel = static_cast<hal::AdcChannelId>(i);
            if (!sample_channel(channel, timestamp_ms)) {
                all_ok = false;
            }
        }
    }
    ++tick_count_;
    return all_ok;
}

void AdcService::reset() {
    snapshot_ = AdcSnapshot{};
    filter_initialized_.fill(false);
    sample_count_ = 0u;
    read_error_count_ = 0u;
    tick_count_ = 0u;
}

const AdcChannelReading& AdcService::channel_reading(hal::AdcChannelId channel) const {
    static const AdcChannelReading kInvalidReading{};
    const auto idx = static_cast<std::size_t>(channel);
    if (idx >= hal::kAdcChannelCount) {
        return kInvalidReading;
    }
    return snapshot_.channels[idx];
}

} // namespace datalogger::bsw
