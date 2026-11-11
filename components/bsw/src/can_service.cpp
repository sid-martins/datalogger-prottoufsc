#include "can_service.hpp"

namespace datalogger::bsw {

CanService::CanService(hal::ICanHal& can_hal)
    : can_hal_(can_hal) {}

bool CanService::initialize(const hal::CanConfig& config) {
    if (!can_hal_.init(config)) {
        initialized_ = false;
        return false;
    }
    if (!can_hal_.start()) {
        initialized_ = false;
        return false;
    }
    initialized_ = true;
    return true;
}

bool CanService::send_frame(const hal::CanFrame& frame, uint32_t timeout_ms) {
    if (!is_frame_valid(frame)) {
        ++tx_error_count_;
        return false;
    }

    if (!can_hal_.transmit(frame, timeout_ms)) {
        ++tx_error_count_;
        return false;
    }

    ++tx_count_;
    return true;
}

bool CanService::receive_to_queue(osal::IOsalQueue<hal::CanFrame>& rx_queue,
                                  uint32_t rx_timeout_ms,
                                  uint32_t queue_timeout_ms) {
    hal::CanFrame frame{};
    if (!can_hal_.receive(frame, rx_timeout_ms)) {
        return false;
    }

    if (!is_frame_valid(frame)) {
        return false;
    }

    ++rx_count_;
    if (!rx_queue.send(frame, queue_timeout_ms)) {
        ++dropped_rx_count_;
        return false;
    }

    return true;
}

bool CanService::check_and_recover_bus() {
    if (can_hal_.get_state() == hal::CanBusState::kBusOff) {
        if (can_hal_.initiate_recovery()) {
            ++bus_off_recovery_count_;
            return true;
        }
    }
    return false;
}

bool CanService::is_frame_valid(const hal::CanFrame& frame) {
    if (frame.dlc > hal::CanFrame::kMaxDlc) {
        return false;
    }
    if (frame.is_extended) {
        if (frame.id > hal::CanFrame::kMaxExtendedId) {
            return false;
        }
    } else {
        if (frame.id > hal::CanFrame::kMaxStandardId) {
            return false;
        }
    }
    return true;
}

} // namespace datalogger::bsw

