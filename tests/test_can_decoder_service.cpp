#include <gtest/gtest.h>
#include "can_decoder_service.hpp"

namespace datalogger::sil {

class CanDecoderServiceTest : public ::testing::Test {
protected:
    bsw::CanDecoderService decoder_;

    static hal::CanFrame make_le_frame(uint32_t id,
                                       uint16_t w0,
                                       uint16_t w1,
                                       uint16_t w2,
                                       uint16_t w3,
                                       uint64_t timestamp_ms = 1000) {
        hal::CanFrame frame{};
        frame.id = id;
        frame.is_extended = false;
        frame.is_rtr = false;
        frame.dlc = 8;
        frame.timestamp_ms = timestamp_ms;
        frame.data = {
            static_cast<uint8_t>(w0 & 0xFFu), static_cast<uint8_t>((w0 >> 8) & 0xFFu),
            static_cast<uint8_t>(w1 & 0xFFu), static_cast<uint8_t>((w1 >> 8) & 0xFFu),
            static_cast<uint8_t>(w2 & 0xFFu), static_cast<uint8_t>((w2 >> 8) & 0xFFu),
            static_cast<uint8_t>(w3 & 0xFFu), static_cast<uint8_t>((w3 >> 8) & 0xFFu),
        };
        return frame;
    }

    static hal::CanFrame make_be_frame(uint32_t id,
                                       bool is_extended,
                                       uint16_t w0,
                                       uint16_t w1,
                                       uint16_t w2,
                                       uint16_t w3,
                                       uint64_t timestamp_ms = 1000) {
        hal::CanFrame frame{};
        frame.id = id;
        frame.is_extended = is_extended;
        frame.is_rtr = false;
        frame.dlc = 8;
        frame.timestamp_ms = timestamp_ms;
        frame.data = {
            static_cast<uint8_t>((w0 >> 8) & 0xFFu), static_cast<uint8_t>(w0 & 0xFFu),
            static_cast<uint8_t>((w1 >> 8) & 0xFFu), static_cast<uint8_t>(w1 & 0xFFu),
            static_cast<uint8_t>((w2 >> 8) & 0xFFu), static_cast<uint8_t>(w2 & 0xFFu),
            static_cast<uint8_t>((w3 >> 8) & 0xFFu), static_cast<uint8_t>(w3 & 0xFFu),
        };
        return frame;
    }
};

TEST_F(CanDecoderServiceTest, DecodesFrontControlsNode0x100LittleEndian) {
    // Ângulo: -45.5 deg (-455), Freio D: 38.5 bar (385), Freio T: 24.0 bar (240), Pedal: 82.5% (825)
    const auto frame = make_le_frame(
        bsw::can_id::kNodeFrontControlsId,
        static_cast<uint16_t>(static_cast<int16_t>(-455)),
        385u,
        240u,
        825u,
        1200u);

    ASSERT_TRUE(decoder_.decode_frame(frame));
    EXPECT_EQ(decoder_.decoded_frame_count(), 1u);

    const auto& fc = decoder_.state().front_controls;
    EXPECT_NEAR(fc.steering_angle_deg, -45.5f, 1e-3f);
    EXPECT_NEAR(fc.brake_pressure_front_bar, 38.5f, 1e-3f);
    EXPECT_NEAR(fc.brake_pressure_rear_bar, 24.0f, 1e-3f);
    EXPECT_NEAR(fc.brake_pedal_pos_pct, 82.5f, 1e-3f);
    EXPECT_TRUE(decoder_.is_node_online(bsw::CanNodeId::kFrontControls, 1300u));
}

TEST_F(CanDecoderServiceTest, DecodesAllFourWheelNodes0x200To0x203) {
    // FL (0x200): Damper=42.5mm (425), Speed=95.4km/h (954), Load=1250N (1250), DiscTemp=345.0C (3450)
    const auto fl_frame = make_le_frame(bsw::can_id::kNodeWheelFlId, 425u, 954u, 1250u, 3450u, 1000u);
    // RR (0x203): Damper=51.0mm (510), Speed=96.0km/h (960), Load=-120N (-120), DiscTemp=410.5C (4105)
    const auto rr_frame = make_le_frame(
        bsw::can_id::kNodeWheelRrId,
        510u,
        960u,
        static_cast<uint16_t>(static_cast<int16_t>(-120)),
        4105u,
        1005u);

    ASSERT_TRUE(decoder_.decode_frame(fl_frame));
    ASSERT_TRUE(decoder_.decode_frame(rr_frame));

    const auto& fl = decoder_.state().wheels[static_cast<std::size_t>(bsw::WheelCorner::kFrontLeft)];
    EXPECT_NEAR(fl.damper_pos_mm, 42.5f, 1e-3f);
    EXPECT_NEAR(fl.wheel_speed_kmh, 95.4f, 1e-3f);
    EXPECT_NEAR(fl.load_cell_n, 1250.0f, 1e-3f);
    EXPECT_NEAR(fl.brake_disc_temp_c, 345.0f, 1e-3f);

    const auto& rr = decoder_.state().wheels[static_cast<std::size_t>(bsw::WheelCorner::kRearRight)];
    EXPECT_NEAR(rr.damper_pos_mm, 51.0f, 1e-3f);
    EXPECT_NEAR(rr.wheel_speed_kmh, 96.0f, 1e-3f);
    EXPECT_NEAR(rr.load_cell_n, -120.0f, 1e-3f);
    EXPECT_NEAR(rr.brake_disc_temp_c, 410.5f, 1e-3f);

    EXPECT_TRUE(decoder_.is_node_online(bsw::CanNodeId::kWheelFl, 1100u));
    EXPECT_TRUE(decoder_.is_node_online(bsw::CanNodeId::kWheelRr, 1100u));
    EXPECT_FALSE(decoder_.is_node_online(bsw::CanNodeId::kWheelFr, 1100u));
}

TEST_F(CanDecoderServiceTest, DecodesImuWitAccel0x300AndGyro0x301) {
    // Accel: Ax=-12.50 m/s^2 (-1250), Ay=18.25 m/s^2 (1825), Az=9.81 m/s^2 (981), Status=0x0001
    const auto accel_frame = make_le_frame(
        bsw::can_id::kNodeImuAccelId,
        static_cast<uint16_t>(static_cast<int16_t>(-1250)),
        1825u,
        981u,
        1u,
        2000u);

    // Gyro: Gx=15.5 deg/s (155), Gy=-8.2 deg/s (-82), Gz=45.0 deg/s (450)
    const auto gyro_frame = make_le_frame(
        bsw::can_id::kNodeImuGyroId,
        155u,
        static_cast<uint16_t>(static_cast<int16_t>(-82)),
        450u,
        0u,
        2002u);

    ASSERT_TRUE(decoder_.decode_frame(accel_frame));
    ASSERT_TRUE(decoder_.decode_frame(gyro_frame));

    const auto& imu = decoder_.state().imu;
    EXPECT_NEAR(imu.accel_x_mps2, -12.50f, 1e-3f);
    EXPECT_NEAR(imu.accel_y_mps2, 18.25f, 1e-3f);
    EXPECT_NEAR(imu.accel_z_mps2, 9.81f, 1e-3f);
    EXPECT_EQ(imu.status_flags, 1u);
    EXPECT_NEAR(imu.gyro_x_dps, 15.5f, 1e-3f);
    EXPECT_NEAR(imu.gyro_y_dps, -8.2f, 1e-3f);
    EXPECT_NEAR(imu.gyro_z_dps, 45.0f, 1e-3f);
    EXPECT_TRUE(decoder_.is_node_online(bsw::CanNodeId::kImuWit, 2100u));
}

TEST_F(CanDecoderServiceTest, DecodesFuelTechEcu29BitExtendedBroadcastMotorAndGear) {
    // Pacote 1 (0x14080600): TPS=98.5% (985), MAP=1.020 bar (1020), AirTemp=32.5C (325), EngineTemp=91.0C (910)
    const auto ft_msg1 = make_be_frame(
        bsw::can_id::kEcuFuelTechExtMsg1Id, true, 985u, 1020u, 325u, 910u, 3000u);

    // Pacote 2 (0x14080601): OilP=4.500 bar (4500), FuelP=3.200 bar (3200), WaterP=1.100 bar (1100), Gear=4 (4ª marcha)
    const auto ft_msg2 = make_be_frame(
        bsw::can_id::kEcuFuelTechExtMsg2Id, true, 4500u, 3200u, 1100u, 4u, 3005u);

    // Pacote 3 (0x14080602): Lambda=0.885 (885), RPM=11250, OilTemp=104.5C (1045), PitLimit=0
    const auto ft_msg3 = make_be_frame(
        bsw::can_id::kEcuFuelTechExtMsg3Id, true, 885u, 11250u, 1045u, 0u, 3010u);

    ASSERT_TRUE(decoder_.decode_frame(ft_msg1));
    ASSERT_TRUE(decoder_.decode_frame(ft_msg2));
    ASSERT_TRUE(decoder_.decode_frame(ft_msg3));

    const auto& ecu = decoder_.state().ecu;
    EXPECT_NEAR(ecu.tps_pct, 98.5f, 1e-3f);
    EXPECT_NEAR(ecu.map_bar, 1.020f, 1e-3f);
    EXPECT_NEAR(ecu.air_temp_c, 32.5f, 1e-3f);
    EXPECT_NEAR(ecu.engine_temp_c, 91.0f, 1e-3f);
    EXPECT_NEAR(ecu.oil_pressure_bar, 4.500f, 1e-3f);
    EXPECT_NEAR(ecu.fuel_pressure_bar, 3.200f, 1e-3f);
    EXPECT_NEAR(ecu.water_pressure_bar, 1.100f, 1e-3f);
    EXPECT_EQ(ecu.gear, 4);
    EXPECT_NEAR(ecu.exhaust_o2_lambda, 0.885f, 1e-4f);
    EXPECT_EQ(ecu.rpm, 11250u);
    EXPECT_NEAR(ecu.oil_temp_c, 104.5f, 1e-3f);
    EXPECT_TRUE(decoder_.is_node_online(bsw::CanNodeId::kEcuFuelTech, 3100u));
}

TEST_F(CanDecoderServiceTest, DetectsNodeTimeoutWhenElapsedExceedsThreshold) {
    const auto frame = make_le_frame(bsw::can_id::kNodeWheelFlId, 100u, 200u, 300u, 400u, 1000u);
    ASSERT_TRUE(decoder_.decode_frame(frame));

    // Dentro da janela de 200 ms -> Online
    EXPECT_TRUE(decoder_.is_node_online(bsw::CanNodeId::kWheelFl, 1200u, 200u));

    // Acima de 200 ms sem pacote -> Offline (Stale)
    EXPECT_FALSE(decoder_.is_node_online(bsw::CanNodeId::kWheelFl, 1201u, 200u));
}

TEST_F(CanDecoderServiceTest, RejectsInvalidDlcRtrAndUnknownIds) {
    hal::CanFrame short_dlc = make_le_frame(bsw::can_id::kNodeFrontControlsId, 1, 2, 3, 4);
    short_dlc.dlc = 7; // Protocolo exige 8 bytes

    EXPECT_FALSE(decoder_.decode_frame(short_dlc));
    EXPECT_EQ(decoder_.decode_error_count(), 1u);

    hal::CanFrame rtr_frame = make_le_frame(bsw::can_id::kNodeFrontControlsId, 1, 2, 3, 4);
    rtr_frame.is_rtr = true;

    EXPECT_FALSE(decoder_.decode_frame(rtr_frame));
    EXPECT_EQ(decoder_.decode_error_count(), 2u);

    hal::CanFrame unknown_frame = make_le_frame(0x7FFu, 1, 2, 3, 4);
    EXPECT_FALSE(decoder_.decode_frame(unknown_frame));
    EXPECT_EQ(decoder_.unknown_id_count(), 1u);
}

} // namespace datalogger::sil
