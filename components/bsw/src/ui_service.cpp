#include "ui_service.hpp"

namespace datalogger::bsw {

UiService::UiService(hal::IGpioHal& gpio_hal)
    : gpio_hal_(gpio_hal) {}

bool UiService::initialize(const UiConfig& config) {
    config_ = config;
    initialized_ = false;
    state_ = UiSystemState::kUninitialized;
    recording_requested_ = false;
    stop_hold_active_ = false;
    stop_hold_start_ms_ = 0ULL;
    lap_beacon_ = 0u;
    sw1_debounce_ = ButtonDebounceState{};
    sw2_debounce_ = ButtonDebounceState{};
    led_acquiring_on_ = false;
    led_working_on_ = false;
    leds_synced_ = false;
    gpio_error_count_ = 0u;

    if (!gpio_hal_.init(config_.gpio)) {
        return false;
    }

    if (!gpio_hal_.write_pin(config_.gpio.led_acquiring_pin, false) ||
        !gpio_hal_.write_pin(config_.gpio.led_working_pin, false)) {
        ++gpio_error_count_;
        return false;
    }

    leds_synced_ = true;
    initialized_ = true;
    state_ = UiSystemState::kStandbyArmed;
    return true;
}

bool UiService::update(const UiSystemInputs& inputs) {
    if (!initialized_) {
        return false;
    }

    update_rpm_state_machine(inputs);
    sample_buttons(inputs.now_ms);
    drive_leds(inputs);
    return true;
}

bool UiService::step_debounce(ButtonDebounceState& state,
                              bool raw_pressed,
                              uint64_t now_ms,
                              uint64_t debounce_ms) {
    if (raw_pressed != state.raw_prev) {
        state.raw_prev = raw_pressed;
        state.candidate_since_ms = now_ms;
    }

    if (raw_pressed != state.stable_pressed) {
        const uint64_t elapsed = (now_ms >= state.candidate_since_ms)
                                     ? (now_ms - state.candidate_since_ms)
                                     : 0ULL;
        if (elapsed >= debounce_ms) {
            const bool was_pressed = state.stable_pressed;
            state.stable_pressed = raw_pressed;
            return !was_pressed && state.stable_pressed;
        }
    }
    return false;
}

void UiService::update_rpm_state_machine(const UiSystemInputs& inputs) {
    const bool engine_running = inputs.ecu_online && (inputs.ecu_rpm > config_.rpm_start_threshold);

    if (!recording_requested_) {
        if (engine_running) {
            recording_requested_ = true;
            stop_hold_active_ = false;
            stop_hold_start_ms_ = 0ULL;
            lap_beacon_ = 0u;
        }
    } else if (engine_running) {
        stop_hold_active_ = false;
        stop_hold_start_ms_ = 0ULL;
    } else if (!stop_hold_active_) {
        if (config_.rpm_stop_hold_ms == 0ULL) {
            recording_requested_ = false;
        } else {
            stop_hold_active_ = true;
            stop_hold_start_ms_ = inputs.now_ms;
        }
    } else {
        const uint64_t elapsed = (inputs.now_ms >= stop_hold_start_ms_)
                                     ? (inputs.now_ms - stop_hold_start_ms_)
                                     : 0ULL;
        if (elapsed >= config_.rpm_stop_hold_ms) {
            recording_requested_ = false;
            stop_hold_active_ = false;
            stop_hold_start_ms_ = 0ULL;
        }
    }

    if (inputs.storage_fault) {
        state_ = UiSystemState::kStorageFault;
    } else if (!recording_requested_) {
        state_ = UiSystemState::kStandbyArmed;
    } else if (stop_hold_active_) {
        state_ = UiSystemState::kStopHold;
    } else {
        state_ = UiSystemState::kRecording;
    }
}

void UiService::sample_buttons(uint64_t now_ms) {
    // Free_SW1 (GPIO 35, ativo baixo): mantido em standby/reservado (sem acionar Start/Stop)
    bool sw1_high = true;
    if (gpio_hal_.read_pin(config_.gpio.sw1_pin, sw1_high)) {
        (void)step_debounce(sw1_debounce_, !sw1_high, now_ms, config_.debounce_ms);
    } else {
        ++gpio_error_count_;
    }

    // Free_SW2 (GPIO 36, ativo baixo): Lap Beacon na borda de pressionamento validada
    bool sw2_high = true;
    if (gpio_hal_.read_pin(config_.gpio.sw2_beacon_pin, sw2_high)) {
        if (step_debounce(sw2_debounce_, !sw2_high, now_ms, config_.debounce_ms)) {
            ++lap_beacon_;
        }
    } else {
        ++gpio_error_count_;
    }
}

void UiService::drive_leds(const UiSystemInputs& inputs) {
    const bool desired_acquiring = inputs.session_active && !inputs.storage_fault;

    uint64_t half_period = config_.heartbeat_half_period_ms;
    if (inputs.storage_fault) {
        half_period = config_.fault_blink_half_period_ms;
    } else if (inputs.can_warning) {
        half_period = config_.can_warn_half_period_ms;
    }
    if (half_period == 0ULL) {
        half_period = 1ULL;
    }
    const bool desired_working = ((inputs.now_ms / half_period) % 2ULL) == 0ULL;

    if (!leds_synced_ || (desired_acquiring != led_acquiring_on_)) {
        if (gpio_hal_.write_pin(config_.gpio.led_acquiring_pin, desired_acquiring)) {
            led_acquiring_on_ = desired_acquiring;
        } else {
            ++gpio_error_count_;
        }
    }

    if (!leds_synced_ || (desired_working != led_working_on_)) {
        if (gpio_hal_.write_pin(config_.gpio.led_working_pin, desired_working)) {
            led_working_on_ = desired_working;
        } else {
            ++gpio_error_count_;
        }
    }

    leds_synced_ = true;
}

} // namespace datalogger::bsw
