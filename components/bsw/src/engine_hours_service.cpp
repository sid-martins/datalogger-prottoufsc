#include "engine_hours_service.hpp"

#include <cstring>
#include <limits>

namespace datalogger::bsw {

EngineHoursService::EngineHoursService(hal::INvmHal& nvm_hal)
    : nvm_hal_(nvm_hal) {
    constexpr const char kDefaultKey[] = "eng_sec_tot";
    std::memcpy(key_buf_.data(), kDefaultKey, sizeof(kDefaultKey));
    config_.key = key_buf_.data();
}

bool EngineHoursService::initialize(const EngineHoursConfig& config) {
    config_ = config;
    if (config_.save_interval_ms == 0ULL) {
        config_.save_interval_ms = 60000ULL;
    }

    const char* src_key = (config.key != nullptr && config.key[0] != '\0')
                              ? config.key
                              : "eng_sec_tot";
    std::size_t key_len = std::strlen(src_key);
    if (key_len >= key_buf_.size()) {
        key_len = key_buf_.size() - 1u;
    }
    std::memcpy(key_buf_.data(), src_key, key_len);
    key_buf_[key_len] = '\0';
    config_.key = key_buf_.data();

    engine_running_ = false;
    last_tick_ms_ = 0ULL;
    total_runtime_ms_ = 0ULL;
    unsaved_run_ms_ = 0ULL;
    persisted_seconds_ = 0u;
    save_count_ = 0u;
    nvm_error_count_ = 0u;
    initialized_ = true;

    if (!nvm_hal_.init(config_.nvm)) {
        nvm_initialized_ = false;
        nvm_healthy_ = false;
        ++nvm_error_count_;
        return false;
    }

    nvm_initialized_ = true;
    uint32_t loaded_seconds = 0u;
    const hal::NvmReadResult res = nvm_hal_.read_u32(config_.key, loaded_seconds);
    if (res == hal::NvmReadResult::kOk) {
        persisted_seconds_ = loaded_seconds;
        total_runtime_ms_ = static_cast<uint64_t>(loaded_seconds) * 1000ULL;
        nvm_healthy_ = true;
        return true;
    }

    if (res == hal::NvmReadResult::kNotFound) {
        persisted_seconds_ = 0u;
        total_runtime_ms_ = 0ULL;
        nvm_healthy_ = true;
        return true;
    }

    nvm_healthy_ = false;
    ++nvm_error_count_;
    return false;
}

void EngineHoursService::update(uint64_t now_ms, uint16_t ecu_rpm, bool ecu_online) {
    if (!initialized_) {
        return;
    }

    const bool running_now = ecu_online && (ecu_rpm > config_.rpm_run_threshold);

    if (running_now) {
        if (!engine_running_) {
            engine_running_ = true;
            last_tick_ms_ = now_ms;
            return;
        }

        uint64_t delta_ms = (now_ms > last_tick_ms_) ? (now_ms - last_tick_ms_) : 0ULL;
        if (config_.max_step_delta_ms > 0ULL && delta_ms > config_.max_step_delta_ms) {
            delta_ms = config_.max_step_delta_ms;
        }
        last_tick_ms_ = now_ms;

        total_runtime_ms_ += delta_ms;
        unsaved_run_ms_ += delta_ms;

        if (unsaved_run_ms_ >= config_.save_interval_ms) {
            unsaved_run_ms_ %= config_.save_interval_ms;
            const uint32_t current_sec = total_seconds();
            if (current_sec != persisted_seconds_) {
                persist_total_seconds(current_sec);
            }
        }
        return;
    }

    if (engine_running_) {
        engine_running_ = false;
        unsaved_run_ms_ = 0ULL;
        const uint32_t current_sec = total_seconds();
        if (current_sec != persisted_seconds_) {
            persist_total_seconds(current_sec);
        }
    }
}

bool EngineHoursService::set_total_seconds(uint32_t total_seconds) {
    if (!initialized_) {
        return false;
    }
    total_runtime_ms_ = static_cast<uint64_t>(total_seconds) * 1000ULL;
    unsaved_run_ms_ = 0ULL;
    return persist_total_seconds(total_seconds);
}

uint32_t EngineHoursService::total_seconds() const {
    const uint64_t secs = total_runtime_ms_ / 1000ULL;
    if (secs > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
        return std::numeric_limits<uint32_t>::max();
    }
    return static_cast<uint32_t>(secs);
}

std::size_t EngineHoursService::format_hhmm(char* out_buf, std::size_t buf_size) const {
    if (out_buf == nullptr || buf_size == 0u) {
        return 0u;
    }

    const uint32_t hours = hours_part();
    const uint8_t mins = minutes_part();

    std::array<char, 10> rev_hours{};
    std::size_t h_digits = 0u;
    uint32_t tmp = hours;
    do {
        rev_hours[h_digits++] = static_cast<char>('0' + (tmp % 10u));
        tmp /= 10u;
    } while (tmp > 0u && h_digits < rev_hours.size());

    while (h_digits < 2u) {
        rev_hours[h_digits++] = '0';
    }

    const std::size_t total_len = h_digits + 3u; // <HH...>:MM
    if (buf_size <= total_len) {
        out_buf[0] = '\0';
        return 0u;
    }

    std::size_t pos = 0u;
    for (std::size_t i = 0u; i < h_digits; ++i) {
        out_buf[pos++] = rev_hours[h_digits - 1u - i];
    }
    out_buf[pos++] = ':';
    out_buf[pos++] = static_cast<char>('0' + (mins / 10u));
    out_buf[pos++] = static_cast<char>('0' + (mins % 10u));
    out_buf[pos] = '\0';
    return pos;
}

bool EngineHoursService::persist_total_seconds(uint32_t seconds_to_save) {
    if (!nvm_initialized_) {
        nvm_healthy_ = false;
        return false;
    }

    if (!nvm_hal_.write_u32(config_.key, seconds_to_save) || !nvm_hal_.commit()) {
        nvm_healthy_ = false;
        ++nvm_error_count_;
        return false;
    }

    persisted_seconds_ = seconds_to_save;
    nvm_healthy_ = true;
    ++save_count_;
    return true;
}

} // namespace datalogger::bsw
