#pragma once

#include "i_can_hal.hpp"

namespace datalogger::lld {

/**
 * @brief LLD (Low-Level Driver / MCAL) do periférico TWAI do ESP32-S3
 * conectado ao transceptor físico SN65HVD230DR (Communication.SchDoc - U2: TX=GPIO 15, RX=GPIO 16).
 */
class CanTwaiLld : public hal::ICanHal {
public:
    CanTwaiLld() = default;
    ~CanTwaiLld() override;

    bool init(const hal::CanConfig& config) override;
    bool start() override;
    bool stop() override;
    bool transmit(const hal::CanFrame& frame, uint32_t timeout_ms) override;
    bool receive(hal::CanFrame& out_frame, uint32_t timeout_ms) override;
    hal::CanBusState get_state() const override;
    bool initiate_recovery() override;

private:
    bool installed_{false};
};

} // namespace datalogger::lld

