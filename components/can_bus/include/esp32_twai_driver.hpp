#pragma once

#include "i_can_driver.hpp"

namespace datalogger::can {

/**
 * @brief Implementação concreta do driver CAN usando o periférico TWAI do ESP32-S3
 * conectado ao transceptor SN65HVD230DR (Communication.SchDoc - U2: TX=GPIO 15, RX=GPIO 16).
 */
class Esp32TwaiDriver : public hal::ICanDriver {
public:
    Esp32TwaiDriver() = default;
    ~Esp32TwaiDriver() override;

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

} // namespace datalogger::can

