#include "can_decoder_service.hpp"

namespace datalogger::bsw {

bool CanDecoderService::decode_frame(const hal::CanFrame& frame) {
    if (frame.is_rtr || frame.dlc < kRequiredFrameDlc) {
        ++decode_error_count_;
        return false;
    }

    if (!frame.is_extended) {
        switch (frame.id) {
            case can_id::kNodeFrontControlsId:
                decode_front_controls(frame);
                return true;
            case can_id::kNodeWheelFlId:
                decode_wheel_node(WheelCorner::kFrontLeft, CanNodeId::kWheelFl, frame);
                return true;
            case can_id::kNodeWheelFrId:
                decode_wheel_node(WheelCorner::kFrontRight, CanNodeId::kWheelFr, frame);
                return true;
            case can_id::kNodeWheelRlId:
                decode_wheel_node(WheelCorner::kRearLeft, CanNodeId::kWheelRl, frame);
                return true;
            case can_id::kNodeWheelRrId:
                decode_wheel_node(WheelCorner::kRearRight, CanNodeId::kWheelRr, frame);
                return true;
            case can_id::kNodeImuAccelId:
                decode_imu_accel(frame);
                return true;
            case can_id::kNodeImuGyroId:
                decode_imu_gyro(frame);
                return true;
            case can_id::kEcuFuelTechMsg1Id:
                decode_fueltech_msg1(frame);
                return true;
            case can_id::kEcuFuelTechMsg2Id:
                decode_fueltech_msg2(frame);
                return true;
            case can_id::kEcuFuelTechMsg3Id:
                decode_fueltech_msg3(frame);
                return true;
            default:
                ++unknown_id_count_;
                return false;
        }
    }

    // Frame Extended (29-bit): Verifica protocolo FuelTech FTCAN 2.0 (MessageID nos 11 bits inferiores)
    const uint32_t message_id = frame.id & can_id::kFuelTechMessageIdMask;
    switch (message_id) {
        case can_id::kEcuFuelTechMsg1Id:
            decode_fueltech_msg1(frame);
            return true;
        case can_id::kEcuFuelTechMsg2Id:
            decode_fueltech_msg2(frame);
            return true;
        case can_id::kEcuFuelTechMsg3Id:
            decode_fueltech_msg3(frame);
            return true;
        default:
            ++unknown_id_count_;
            return false;
    }
}

bool CanDecoderService::is_node_online(CanNodeId node,
                                       uint64_t current_time_ms,
                                       uint64_t timeout_ms) const {
    const auto idx = static_cast<std::size_t>(node);
    if (idx >= VehicleCanState::kNodeCount) {
        return false;
    }
    if (!state_.node_ever_received[idx]) {
        return false;
    }
    const uint64_t last_rx = state_.last_rx_timestamp_ms[idx];
    if (current_time_ms < last_rx) {
        return true;
    }
    return (current_time_ms - last_rx) <= timeout_ms;
}

void CanDecoderService::reset() {
    state_ = VehicleCanState{};
    decoded_frame_count_ = 0;
    unknown_id_count_ = 0;
    decode_error_count_ = 0;
}

uint16_t CanDecoderService::unpack_u16_le(const uint8_t* bytes) {
    return static_cast<uint16_t>(static_cast<uint16_t>(bytes[0]) |
                                 (static_cast<uint16_t>(bytes[1]) << 8u));
}

int16_t CanDecoderService::unpack_i16_le(const uint8_t* bytes) {
    return static_cast<int16_t>(unpack_u16_le(bytes));
}

uint16_t CanDecoderService::unpack_u16_be(const uint8_t* bytes) {
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8u) |
                                 static_cast<uint16_t>(bytes[1]));
}

int16_t CanDecoderService::unpack_i16_be(const uint8_t* bytes) {
    return static_cast<int16_t>(unpack_u16_be(bytes));
}

void CanDecoderService::mark_node_received(CanNodeId node, uint64_t timestamp_ms) {
    const auto idx = static_cast<std::size_t>(node);
    state_.last_rx_timestamp_ms[idx] = timestamp_ms;
    state_.node_ever_received[idx] = true;
    ++decoded_frame_count_;
}

void CanDecoderService::decode_front_controls(const hal::CanFrame& frame) {
    const uint8_t* d = frame.data.data();
    state_.front_controls.steering_angle_deg =
        static_cast<float>(unpack_i16_le(&d[0])) * 0.1f;
    state_.front_controls.brake_pressure_front_bar =
        static_cast<float>(unpack_u16_le(&d[2])) * 0.1f;
    state_.front_controls.brake_pressure_rear_bar =
        static_cast<float>(unpack_u16_le(&d[4])) * 0.1f;
    state_.front_controls.brake_pedal_pos_pct =
        static_cast<float>(unpack_u16_le(&d[6])) * 0.1f;

    mark_node_received(CanNodeId::kFrontControls, frame.timestamp_ms);
}

void CanDecoderService::decode_wheel_node(WheelCorner corner,
                                          CanNodeId node,
                                          const hal::CanFrame& frame) {
    const uint8_t* d = frame.data.data();
    auto& wheel = state_.wheels[static_cast<std::size_t>(corner)];
    wheel.damper_pos_mm     = static_cast<float>(unpack_u16_le(&d[0])) * 0.1f;
    wheel.wheel_speed_kmh   = static_cast<float>(unpack_u16_le(&d[2])) * 0.1f;
    wheel.load_cell_n       = static_cast<float>(unpack_i16_le(&d[4]));
    wheel.brake_disc_temp_c = static_cast<float>(unpack_i16_le(&d[6])) * 0.1f;

    mark_node_received(node, frame.timestamp_ms);
}

void CanDecoderService::decode_imu_accel(const hal::CanFrame& frame) {
    const uint8_t* d = frame.data.data();
    state_.imu.accel_x_mps2 = static_cast<float>(unpack_i16_le(&d[0])) * 0.01f;
    state_.imu.accel_y_mps2 = static_cast<float>(unpack_i16_le(&d[2])) * 0.01f;
    state_.imu.accel_z_mps2 = static_cast<float>(unpack_i16_le(&d[4])) * 0.01f;
    state_.imu.status_flags = unpack_u16_le(&d[6]);

    mark_node_received(CanNodeId::kImuWit, frame.timestamp_ms);
}

void CanDecoderService::decode_imu_gyro(const hal::CanFrame& frame) {
    const uint8_t* d = frame.data.data();
    state_.imu.gyro_x_dps = static_cast<float>(unpack_i16_le(&d[0])) * 0.1f;
    state_.imu.gyro_y_dps = static_cast<float>(unpack_i16_le(&d[2])) * 0.1f;
    state_.imu.gyro_z_dps = static_cast<float>(unpack_i16_le(&d[4])) * 0.1f;

    mark_node_received(CanNodeId::kImuWit, frame.timestamp_ms);
}

void CanDecoderService::decode_fueltech_msg1(const hal::CanFrame& frame) {
    const uint8_t* d = frame.data.data();
    state_.ecu.tps_pct       = static_cast<float>(unpack_i16_be(&d[0])) * 0.1f;
    state_.ecu.map_bar       = static_cast<float>(unpack_i16_be(&d[2])) * 0.001f;
    state_.ecu.air_temp_c    = static_cast<float>(unpack_i16_be(&d[4])) * 0.1f;
    state_.ecu.engine_temp_c = static_cast<float>(unpack_i16_be(&d[6])) * 0.1f;

    mark_node_received(CanNodeId::kEcuFuelTech, frame.timestamp_ms);
}

void CanDecoderService::decode_fueltech_msg2(const hal::CanFrame& frame) {
    const uint8_t* d = frame.data.data();
    state_.ecu.oil_pressure_bar   = static_cast<float>(unpack_i16_be(&d[0])) * 0.001f;
    state_.ecu.fuel_pressure_bar  = static_cast<float>(unpack_i16_be(&d[2])) * 0.001f;
    state_.ecu.water_pressure_bar = static_cast<float>(unpack_i16_be(&d[4])) * 0.001f;
    state_.ecu.gear               = unpack_i16_be(&d[6]);

    mark_node_received(CanNodeId::kEcuFuelTech, frame.timestamp_ms);
}

void CanDecoderService::decode_fueltech_msg3(const hal::CanFrame& frame) {
    const uint8_t* d = frame.data.data();
    state_.ecu.exhaust_o2_lambda = static_cast<float>(unpack_i16_be(&d[0])) * 0.001f;
    state_.ecu.rpm               = unpack_u16_be(&d[2]);
    state_.ecu.oil_temp_c        = static_cast<float>(unpack_i16_be(&d[4])) * 0.1f;
    state_.ecu.pit_limit_status  = unpack_u16_be(&d[6]);

    mark_node_received(CanNodeId::kEcuFuelTech, frame.timestamp_ms);
}

} // namespace datalogger::bsw
