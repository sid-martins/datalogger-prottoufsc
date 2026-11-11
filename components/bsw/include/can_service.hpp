#pragma once

#include <cstdint>
#include "i_can_hal.hpp"
#include "i_osal_queue.hpp"

namespace datalogger::bsw {

/**
 * @brief BSW (Basic Software Service) - Serviço de gerenciamento e diagnóstico do barramento CAN.
 *
 * Opera de forma 100% independente do silício através das interfaces `hal::ICanHal` e `osal::IOsalQueue`,
 * permitindo validação completa em ambiente SIL (Software-in-the-Loop) no PC.
 */
class CanService {
public:
    explicit CanService(hal::ICanHal& can_hal);

    bool initialize(const hal::CanConfig& config = hal::CanConfig{});

    bool send_frame(const hal::CanFrame& frame, uint32_t timeout_ms = 50);

    bool receive_to_queue(osal::IOsalQueue<hal::CanFrame>& rx_queue,
                          uint32_t rx_timeout_ms = 100,
                          uint32_t queue_timeout_ms = 10);

    bool check_and_recover_bus();

    static bool is_frame_valid(const hal::CanFrame& frame);

    bool is_initialized() const { return initialized_; }
    uint32_t tx_count() const { return tx_count_; }
    uint32_t rx_count() const { return rx_count_; }
    uint32_t tx_error_count() const { return tx_error_count_; }
    uint32_t dropped_rx_count() const { return dropped_rx_count_; }
    uint32_t bus_off_recovery_count() const { return bus_off_recovery_count_; }

private:
    hal::ICanHal& can_hal_;
    bool initialized_{false};
    uint32_t tx_count_{0};
    uint32_t rx_count_{0};
    uint32_t tx_error_count_{0};
    uint32_t dropped_rx_count_{0};
    uint32_t bus_off_recovery_count_{0};
};

} // namespace datalogger::bsw

