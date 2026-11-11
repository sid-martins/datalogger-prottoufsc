#include "esp32_twai_driver.hpp"

#include <cstring>
#include "driver/twai.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

namespace datalogger::can {

namespace {
constexpr const char* kTag = "CAN_TWAI";

twai_mode_t to_twai_mode(hal::CanOperatingMode mode) {
    switch (mode) {
        case hal::CanOperatingMode::kListenOnly:
            return TWAI_MODE_LISTEN_ONLY;
        case hal::CanOperatingMode::kNoAck:
            return TWAI_MODE_NO_ACK;
        case hal::CanOperatingMode::kNormal:
        default:
            return TWAI_MODE_NORMAL;
    }
}

twai_timing_config_t to_twai_timing(hal::CanBaudRate baud_rate) {
    switch (baud_rate) {
        case hal::CanBaudRate::k250Kbps:
            return TWAI_TIMING_CONFIG_250KBITS();
        case hal::CanBaudRate::k1Mbps:
            return TWAI_TIMING_CONFIG_1MBITS();
        case hal::CanBaudRate::k500Kbps:
        default:
            return TWAI_TIMING_CONFIG_500KBITS();
    }
}
} // namespace

Esp32TwaiDriver::~Esp32TwaiDriver() {
    if (installed_) {
        twai_stop();
        twai_driver_uninstall();
    }
}

bool Esp32TwaiDriver::init(const hal::CanConfig& config) {
    if (installed_) {
        twai_stop();
        twai_driver_uninstall();
        installed_ = false;
    }

    twai_general_config_t g_config =
        TWAI_GENERAL_CONFIG_DEFAULT(config.tx_pin, config.rx_pin, to_twai_mode(config.mode));
    g_config.tx_queue_len = config.tx_queue_len;
    g_config.rx_queue_len = config.rx_queue_len;

    const twai_timing_config_t t_config = to_twai_timing(config.baud_rate);
    const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        ESP_LOGE(kTag, "Falha ao instalar driver TWAI (TX=GPIO %d, RX=GPIO %d)",
                 static_cast<int>(config.tx_pin), static_cast<int>(config.rx_pin));
        return false;
    }

    installed_ = true;
    ESP_LOGI(kTag, "Driver TWAI instalado para SN65HVD230DR (TX=GPIO %d, RX=GPIO %d, %lu bps)",
             static_cast<int>(config.tx_pin),
             static_cast<int>(config.rx_pin),
             static_cast<unsigned long>(config.baud_rate));
    return true;
}

bool Esp32TwaiDriver::start() {
    if (!installed_) {
        return false;
    }
    return twai_start() == ESP_OK;
}

bool Esp32TwaiDriver::stop() {
    if (!installed_) {
        return false;
    }
    return twai_stop() == ESP_OK;
}

bool Esp32TwaiDriver::transmit(const hal::CanFrame& frame, uint32_t timeout_ms) {
    if (!installed_ || frame.dlc > TWAI_FRAME_MAX_DLC) {
        return false;
    }

    twai_message_t msg{};
    msg.identifier = frame.id;
    msg.data_length_code = frame.dlc;
    msg.flags = TWAI_MSG_FLAG_NONE;
    if (frame.is_extended) {
        msg.flags |= TWAI_MSG_FLAG_EXTD;
    }
    if (frame.is_rtr) {
        msg.flags |= TWAI_MSG_FLAG_RTR;
    }
    std::memcpy(msg.data, frame.data.data(), frame.dlc);

    return twai_transmit(&msg, pdMS_TO_TICKS(timeout_ms)) == ESP_OK;
}

bool Esp32TwaiDriver::receive(hal::CanFrame& out_frame, uint32_t timeout_ms) {
    if (!installed_) {
        return false;
    }

    twai_message_t msg{};
    if (twai_receive(&msg, pdMS_TO_TICKS(timeout_ms)) != ESP_OK) {
        return false;
    }

    out_frame.id = msg.identifier;
    out_frame.is_extended = (msg.flags & TWAI_MSG_FLAG_EXTD) != 0;
    out_frame.is_rtr = (msg.flags & TWAI_MSG_FLAG_RTR) != 0;
    out_frame.dlc = msg.data_length_code <= hal::CanFrame::kMaxDlc
                        ? msg.data_length_code
                        : hal::CanFrame::kMaxDlc;
    out_frame.data.fill(0);
    std::memcpy(out_frame.data.data(), msg.data, out_frame.dlc);
    out_frame.timestamp_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);

    return true;
}

hal::CanBusState Esp32TwaiDriver::get_state() const {
    if (!installed_) {
        return hal::CanBusState::kStopped;
    }

    twai_status_info_t status{};
    if (twai_get_status_info(&status) != ESP_OK) {
        return hal::CanBusState::kStopped;
    }

    switch (status.state) {
        case TWAI_STATE_RUNNING:
            return hal::CanBusState::kRunning;
        case TWAI_STATE_BUS_OFF:
            return hal::CanBusState::kBusOff;
        case TWAI_STATE_RECOVERING:
            return hal::CanBusState::kRecovering;
        case TWAI_STATE_STOPPED:
        default:
            return hal::CanBusState::kStopped;
    }
}

bool Esp32TwaiDriver::initiate_recovery() {
    if (!installed_) {
        return false;
    }
    return twai_initiate_recovery() == ESP_OK;
}

} // namespace datalogger::can

