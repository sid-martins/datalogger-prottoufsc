#pragma once

#include <array>
#include <cstdint>
#include "adc_service.hpp"
#include "can_protocol.hpp"
#include "i_adc_hal.hpp"

namespace datalogger::bsw {

/**
 * @brief Snapshot unificado de telemetria capturado no Core 1 e enviado ao Core 0 (zero heap).
 *
 * Consolida a base temporal (`RtcService`), os 10 canais analógicos locais (`AdcService`),
 * o estado decodificado da rede CAN distribuída (`CanDecoderService`: 6 nós ESP32-C3 + ECU FuelTech)
 * e os dados de posicionamento/velocidade GPS (`GpsService`).
 */
struct TelemetrySnapshot {
    uint64_t monotonic_ms{0};
    uint64_t epoch_ms{0};
    uint32_t sample_seq{0};
    uint16_t lap_beacon{0};
    uint16_t adc_valid_mask{0};
    uint8_t can_node_online_mask{0};
    bool rtc_synchronized{false};

    std::array<float, hal::kAdcChannelCount> adc_physical{};

    FrontControlsData front_controls{};
    std::array<WheelNodeData, VehicleCanState::kWheelCount> wheels{};
    ImuWitData imu{};
    EcuFuelTechData ecu{};

    double gps_latitude_deg{0.0};
    double gps_longitude_deg{0.0};
    float gps_altitude_m{0.0f};
    float gps_speed_kmh{0.0f};
    float gps_speed_mps{0.0f};
    float gps_course_deg{0.0f};
    uint8_t gps_satellites{0};
    bool gps_has_fix{false};
};

} // namespace datalogger::bsw
