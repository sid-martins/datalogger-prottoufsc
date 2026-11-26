#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "adc_service.hpp"
#include "binary_log_format.hpp"
#include "bsp_pins.h"
#include "can_decoder_service.hpp"
#include "fake_osal_queue.hpp"
#include "gps_service.hpp"
#include "logger_service.hpp"
#include "mock_adc_hal.hpp"
#include "mock_gps_hal.hpp"
#include "mock_rtc_hal.hpp"
#include "mock_storage_hal.hpp"
#include "rtc_service.hpp"
#include "sd_card_lld.hpp"
#include "telemetry_aggregator.hpp"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgReferee;

namespace datalogger::sil {

namespace {

std::string make_nmea_sentence(const char* body_without_dollar_or_star) {
    uint8_t xor_sum = 0u;
    for (const char* p = body_without_dollar_or_star; *p != '\0'; ++p) {
        xor_sum = static_cast<uint8_t>(xor_sum ^ static_cast<uint8_t>(*p));
    }
    char buf[160];
    std::snprintf(buf, sizeof(buf), "$%s*%02X\r\n", body_without_dollar_or_star, xor_sum);
    return std::string(buf);
}

hal::CanFrame make_can_le(uint32_t id,
                          uint16_t w0,
                          uint16_t w1,
                          uint16_t w2,
                          uint16_t w3,
                          uint64_t timestamp_ms) {
    hal::CanFrame frame{};
    frame.id = id;
    frame.is_extended = false;
    frame.is_rtr = false;
    frame.dlc = 8;
    frame.timestamp_ms = timestamp_ms;
    frame.data = {
        static_cast<uint8_t>(w0 & 0xFFu), static_cast<uint8_t>((w0 >> 8u) & 0xFFu),
        static_cast<uint8_t>(w1 & 0xFFu), static_cast<uint8_t>((w1 >> 8u) & 0xFFu),
        static_cast<uint8_t>(w2 & 0xFFu), static_cast<uint8_t>((w2 >> 8u) & 0xFFu),
        static_cast<uint8_t>(w3 & 0xFFu), static_cast<uint8_t>((w3 >> 8u) & 0xFFu)
    };
    return frame;
}

hal::CanFrame make_can_be(uint32_t id,
                          bool is_extended,
                          uint16_t w0,
                          uint16_t w1,
                          uint16_t w2,
                          uint16_t w3,
                          uint64_t timestamp_ms) {
    hal::CanFrame frame{};
    frame.id = id;
    frame.is_extended = is_extended;
    frame.is_rtr = false;
    frame.dlc = 8;
    frame.timestamp_ms = timestamp_ms;
    frame.data = {
        static_cast<uint8_t>((w0 >> 8u) & 0xFFu), static_cast<uint8_t>(w0 & 0xFFu),
        static_cast<uint8_t>((w1 >> 8u) & 0xFFu), static_cast<uint8_t>(w1 & 0xFFu),
        static_cast<uint8_t>((w2 >> 8u) & 0xFFu), static_cast<uint8_t>(w2 & 0xFFu),
        static_cast<uint8_t>((w3 >> 8u) & 0xFFu), static_cast<uint8_t>(w3 & 0xFFu)
    };
    return frame;
}

uint16_t read_u16_le(const uint8_t* src, std::size_t offset) {
    return static_cast<uint16_t>(src[offset]) |
           static_cast<uint16_t>(static_cast<uint16_t>(src[offset + 1u]) << 8u);
}

int16_t read_i16_le(const uint8_t* src, std::size_t offset) {
    return static_cast<int16_t>(read_u16_le(src, offset));
}

uint32_t read_u32_le(const uint8_t* src, std::size_t offset) {
    return static_cast<uint32_t>(src[offset]) |
           (static_cast<uint32_t>(src[offset + 1u]) << 8u) |
           (static_cast<uint32_t>(src[offset + 2u]) << 16u) |
           (static_cast<uint32_t>(src[offset + 3u]) << 24u);
}

uint64_t read_u64_le(const uint8_t* src, std::size_t offset) {
    uint64_t value = 0ULL;
    for (std::size_t i = 0u; i < 8u; ++i) {
        value |= static_cast<uint64_t>(src[offset + i]) << (i * 8u);
    }
    return value;
}

float read_f32_le(const uint8_t* src, std::size_t offset) {
    const uint32_t bits = read_u32_le(src, offset);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

double read_f64_le(const uint8_t* src, std::size_t offset) {
    const uint64_t bits = read_u64_le(src, offset);
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

} // namespace

class LoggerAndAggregatorTest : public ::testing::Test {
protected:
    NiceMock<MockRtcHal> mock_rtc_hal_;
    NiceMock<MockAdcHal> mock_adc_hal_;
    NiceMock<MockGpsHal> mock_gps_hal_;
    NiceMock<MockStorageHal> mock_storage_hal_;

    bsw::RtcService rtc_service_{mock_rtc_hal_};
    bsw::CanDecoderService can_decoder_;
    bsw::AdcService adc_service_{mock_adc_hal_};
    bsw::GpsService gps_service_{mock_gps_hal_, rtc_service_};
    bsw::TelemetryAggregator aggregator_{rtc_service_, can_decoder_, adc_service_, gps_service_};

    void init_synchronized_rtc(uint64_t monotonic_ms) {
        const hal::RtcDateTime dt{2026u, 11u, 19u, 4u, 14u, 30u, 5u};
        const hal::RtcDeviceStatus status{false};
        ON_CALL(mock_rtc_hal_, init(_)).WillByDefault(Return(true));
        ON_CALL(mock_rtc_hal_, read_datetime(_, _))
            .WillByDefault(DoAll(SetArgReferee<0>(dt), SetArgReferee<1>(status), Return(true)));
        ON_CALL(mock_rtc_hal_, get_monotonic_ms()).WillByDefault(Return(monotonic_ms));
        ASSERT_TRUE(rtc_service_.initialize());
    }

    void init_degraded_rtc(uint64_t monotonic_ms) {
        ON_CALL(mock_rtc_hal_, init(_)).WillByDefault(Return(false));
        ON_CALL(mock_rtc_hal_, get_monotonic_ms()).WillByDefault(Return(monotonic_ms));
        ASSERT_FALSE(rtc_service_.initialize());
        ASSERT_TRUE(rtc_service_.is_degraded());
    }
};

TEST_F(LoggerAndAggregatorTest, AggregatorCapturesUnifiedSnapshotFromCanAdcGpsAndRtc) {
    init_synchronized_rtc(5000ULL);

    ON_CALL(mock_adc_hal_, init()).WillByDefault(Return(true));
    ASSERT_TRUE(adc_service_.initialize());
    ASSERT_TRUE(adc_service_.set_channel_calibration(hal::AdcChannelId::kOilPressure, 10.0f / 3300.0f, 0.0f));
    for (std::size_t i = 0u; i < hal::kAdcChannelCount; ++i) {
        const int mv = (i == 0u) ? 1650 : 1000;
        ON_CALL(mock_adc_hal_, read_millivolts(static_cast<hal::AdcChannelId>(i), _))
            .WillByDefault(DoAll(SetArgReferee<1>(mv), Return(true)));
    }
    ASSERT_TRUE(adc_service_.sample_all(5000ULL));

    // Popula os 6 nós C3 + ECU FuelTech na janela <= 200 ms de t=5000 ms
    ASSERT_TRUE(can_decoder_.decode_frame(
        make_can_le(bsw::can_id::kNodeFrontControlsId, static_cast<uint16_t>(static_cast<int16_t>(-150)), 400u, 200u, 500u, 4950ULL)));
    ASSERT_TRUE(can_decoder_.decode_frame(make_can_le(bsw::can_id::kNodeWheelFlId, 300u, 800u, 1100u, 3200u, 4960ULL)));
    ASSERT_TRUE(can_decoder_.decode_frame(make_can_le(bsw::can_id::kNodeWheelFrId, 310u, 805u, 1150u, 3250u, 4965ULL)));
    ASSERT_TRUE(can_decoder_.decode_frame(make_can_le(bsw::can_id::kNodeWheelRlId, 280u, 795u, 900u, 2900u, 4970ULL)));
    ASSERT_TRUE(can_decoder_.decode_frame(make_can_le(bsw::can_id::kNodeWheelRrId, 285u, 798u, 920u, 2950u, 4975ULL)));
    ASSERT_TRUE(can_decoder_.decode_frame(make_can_le(bsw::can_id::kNodeImuAccelId, 120u, static_cast<uint16_t>(static_cast<int16_t>(-250)), 981u, 1u, 4980ULL)));
    ASSERT_TRUE(can_decoder_.decode_frame(make_can_le(bsw::can_id::kNodeImuGyroId, 10u, 20u, 30u, 0u, 4985ULL)));
    ASSERT_TRUE(can_decoder_.decode_frame(make_can_be(bsw::can_id::kEcuFuelTechExtMsg1Id, true, 750u, 1010u, 300u, 900u, 4990ULL)));
    ASSERT_TRUE(can_decoder_.decode_frame(make_can_be(bsw::can_id::kEcuFuelTechExtMsg2Id, true, 4200u, 3100u, 1050u, 3u, 4992ULL)));
    ASSERT_TRUE(can_decoder_.decode_frame(make_can_be(bsw::can_id::kEcuFuelTechExtMsg3Id, true, 900u, 9800u, 1020u, 0u, 4995ULL)));

    ON_CALL(mock_gps_hal_, init(_)).WillByDefault(Return(true));
    ASSERT_TRUE(gps_service_.initialize());
    const std::string gga = make_nmea_sentence("GPGGA,143005.00,2736.0000,S,04831.0000,W,1,09,0.9,12.5,M,-1.0,M,,");
    const std::string rmc = make_nmea_sentence("GPRMC,143005.00,A,2736.0000,S,04831.0000,W,045.0,180.5,191126,000.0,W");
    ASSERT_TRUE(gps_service_.parse_nmea_sentence(gga));
    ASSERT_TRUE(gps_service_.parse_nmea_sentence(rmc));

    const bsw::TelemetrySnapshot snap = aggregator_.capture_snapshot(7u);
    EXPECT_EQ(snap.monotonic_ms, 5000ULL);
    EXPECT_TRUE(snap.rtc_synchronized);
    EXPECT_EQ(snap.epoch_ms, bsw::RtcService::datetime_to_epoch_ms({2026u, 11u, 19u, 4u, 14u, 30u, 5u}));
    EXPECT_EQ(snap.sample_seq, 1u);
    EXPECT_EQ(snap.lap_beacon, 7u);
    EXPECT_EQ(snap.adc_valid_mask, 0x03FFu);
    EXPECT_EQ(snap.can_node_online_mask, 0x7Fu);
    EXPECT_NEAR(snap.adc_physical[0], 5.0f, 1e-3f);
    EXPECT_NEAR(snap.front_controls.steering_angle_deg, -15.0f, 1e-3f);
    EXPECT_NEAR(snap.wheels[0].wheel_speed_kmh, 80.0f, 1e-3f);
    EXPECT_NEAR(snap.wheels[3].brake_disc_temp_c, 295.0f, 1e-3f);
    EXPECT_NEAR(snap.imu.accel_y_mps2, -2.50f, 1e-3f);
    EXPECT_EQ(snap.ecu.rpm, 9800u);
    EXPECT_EQ(snap.ecu.gear, 3);
    EXPECT_TRUE(snap.gps_has_fix);
    EXPECT_EQ(snap.gps_satellites, 9u);
    EXPECT_NEAR(snap.gps_latitude_deg, -27.6, 1e-4);
    EXPECT_NEAR(snap.gps_speed_kmh, 45.0f * bsw::GpsService::kKnotsToKmh, 1e-2f);
}

TEST_F(LoggerAndAggregatorTest, AggregatorPublishesToOsalQueueAndCountsDropsWhenFull) {
    init_synchronized_rtc(1000ULL);
    FakeOsalQueue<bsw::TelemetrySnapshot> queue(1u);

    EXPECT_TRUE(aggregator_.publish_snapshot(queue, 0u, 1u));
    EXPECT_FALSE(aggregator_.publish_snapshot(queue, 0u, 2u));

    EXPECT_EQ(aggregator_.sample_seq(), 2u);
    EXPECT_EQ(aggregator_.published_count(), 1u);
    EXPECT_EQ(aggregator_.dropped_count(), 1u);
    EXPECT_EQ(queue.messages_waiting(), 1u);

    bsw::TelemetrySnapshot dequeued{};
    ASSERT_TRUE(queue.receive(dequeued, 0u));
    EXPECT_EQ(dequeued.sample_seq, 1u);
    EXPECT_EQ(dequeued.lap_beacon, 1u);
}

TEST_F(LoggerAndAggregatorTest, AggregatorMarksTimedOutCanNodeOfflineInMask) {
    init_synchronized_rtc(1000ULL);
    ASSERT_TRUE(can_decoder_.decode_frame(make_can_le(bsw::can_id::kNodeWheelFlId, 100u, 200u, 300u, 400u, 1000ULL)));
    ASSERT_TRUE(can_decoder_.decode_frame(make_can_le(bsw::can_id::kNodeFrontControlsId, 10u, 20u, 30u, 40u, 1150ULL)));

    // Avança tempo monotônico para 1250 ms: WheelFl (250 ms) > 200 ms offline, FrontControls (100 ms) <= 200 ms online
    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms()).WillRepeatedly(Return(1250ULL));
    const bsw::TelemetrySnapshot snap = aggregator_.capture_snapshot(0u, 200ULL);

    const uint8_t front_bit = static_cast<uint8_t>(1u << static_cast<uint8_t>(bsw::CanNodeId::kFrontControls));
    const uint8_t wheel_fl_bit = static_cast<uint8_t>(1u << static_cast<uint8_t>(bsw::CanNodeId::kWheelFl));
    EXPECT_NE((snap.can_node_online_mask & front_bit), 0u);
    EXPECT_EQ((snap.can_node_online_mask & wheel_fl_bit), 0u);
}

TEST_F(LoggerAndAggregatorTest, Crc32MatchesStandardIeee8023TestVector) {
    const std::array<uint8_t, 9> ascii_digits{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    EXPECT_EQ(bsw::LoggerService::compute_crc32(ascii_digits.data(), ascii_digits.size()), 0xCBF43926u);
    EXPECT_EQ(bsw::LoggerService::compute_crc32(nullptr, 5u), 0u);
    EXPECT_EQ(bsw::LoggerService::compute_crc32(ascii_digits.data(), 0u), 0u);

    auto corrupted = ascii_digits;
    corrupted[4] ^= 0x01u;
    EXPECT_NE(bsw::LoggerService::compute_crc32(corrupted.data(), corrupted.size()), 0xCBF43926u);
}

TEST_F(LoggerAndAggregatorTest, BinaryFileHeaderEncodesMagicMetadataAllTenBspTagsAndValidCrc) {
    std::array<uint8_t, bsw::LoggerService::kFileHeaderSize> small_buf{};
    EXPECT_EQ(bsw::LoggerService::encode_file_header(bsw::binlog::BinarySessionHeaderMeta{}, nullptr, 256u), 0u);
    EXPECT_EQ(bsw::LoggerService::encode_file_header(
                  bsw::binlog::BinarySessionHeaderMeta{},
                  small_buf.data(),
                  bsw::LoggerService::kFileHeaderSize - 1u),
              0u);

    bsw::binlog::BinarySessionHeaderMeta meta{};
    meta.start_monotonic_ms = 0x1122334455667788ULL;
    meta.start_epoch_ms = 1795098605123ULL;
    meta.nominal_rate_hz = 100u;
    meta.rtc_synchronized = true;
    meta.engine_hours_total_s = 450525u; // 125h 08m 45s

    std::array<uint8_t, bsw::LoggerService::kFileHeaderSize> hdr{};
    hdr.fill(0xAAu);
    ASSERT_EQ(bsw::LoggerService::encode_file_header(meta, hdr.data(), hdr.size()),
              bsw::LoggerService::kFileHeaderSize);

    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(hdr.data()), 4u), "PDLG");
    EXPECT_EQ(read_u16_le(hdr.data(), 4u), 1u);
    EXPECT_EQ(read_u16_le(hdr.data(), 6u), 256u);
    EXPECT_EQ(read_u16_le(hdr.data(), 8u), 256u);
    EXPECT_EQ(read_u16_le(hdr.data(), 10u), bsw::binlog::kHeaderFlagRtcSynced);
    EXPECT_EQ(read_u16_le(hdr.data(), 12u), 100u);
    EXPECT_EQ(hdr[14], 10u);
    EXPECT_EQ(hdr[15], 7u);
    EXPECT_EQ(read_u64_le(hdr.data(), 16u), meta.start_monotonic_ms);
    EXPECT_EQ(read_u64_le(hdr.data(), 24u), meta.start_epoch_ms);

    for (std::size_t i = 0u; i < hal::kAdcChannelCount; ++i) {
        const std::size_t slot_offset = 32u + i * bsw::binlog::kAdcTagSlotSize;
        const char* slot_ptr = reinterpret_cast<const char*>(hdr.data() + slot_offset);
        const std::size_t tag_len = std::strlen(bsp::kAnalogChannels[i].tag);
        EXPECT_EQ(std::string_view(slot_ptr), std::string_view(bsp::kAnalogChannels[i].tag));
        for (std::size_t p = tag_len; p < bsw::binlog::kAdcTagSlotSize; ++p) {
            EXPECT_EQ(hdr[slot_offset + p], 0u);
        }
    }

    const char* dev_ptr = reinterpret_cast<const char*>(hdr.data() + 192u);
    EXPECT_EQ(std::string_view(dev_ptr), bsw::binlog::kDeviceId);
    for (std::size_t p = bsw::binlog::kDeviceId.size(); p < bsw::binlog::kDeviceIdSlotSize; ++p) {
        EXPECT_EQ(hdr[192u + p], 0u);
    }
    EXPECT_EQ(read_u32_le(hdr.data(), 224u), 450525u);
    for (std::size_t p = 228u; p < 252u; ++p) {
        EXPECT_EQ(hdr[p], 0u);
    }

    const uint32_t expected_crc = bsw::LoggerService::compute_crc32(hdr.data(), 252u);
    EXPECT_EQ(read_u32_le(hdr.data(), 252u), expected_crc);

    // Verifica flag zerado quando RTC estiver em modo degradado
    meta.rtc_synchronized = false;
    ASSERT_EQ(bsw::LoggerService::encode_file_header(meta, hdr.data(), hdr.size()), 256u);
    EXPECT_EQ(read_u16_le(hdr.data(), 10u), 0u);
}

TEST_F(LoggerAndAggregatorTest, BinaryRecordEncodesAllSnapshotFieldsAtExactOffsetsWithValidCrc) {
    std::array<uint8_t, bsw::LoggerService::kRecordSize> rec{};
    EXPECT_EQ(bsw::LoggerService::encode_telemetry_record(bsw::TelemetrySnapshot{}, nullptr, 256u), 0u);
    EXPECT_EQ(bsw::LoggerService::encode_telemetry_record(bsw::TelemetrySnapshot{}, rec.data(), 255u), 0u);

    bsw::TelemetrySnapshot snap{};
    snap.monotonic_ms = 987654321ULL;
    snap.epoch_ms = 1795098605432ULL;
    snap.sample_seq = 4242u;
    snap.lap_beacon = 12u;
    snap.adc_valid_mask = 0x02ABu;
    snap.can_node_online_mask = 0x5Fu;
    snap.rtc_synchronized = true;
    snap.gps_has_fix = true;
    snap.gps_satellites = 11u;

    for (std::size_t i = 0u; i < hal::kAdcChannelCount; ++i) {
        snap.adc_physical[i] = static_cast<float>(i) * 1.25f - 2.0f;
    }

    snap.front_controls = {-15.5f, 42.25f, 18.75f, 66.5f};
    snap.wheels[0] = {12.5f, 81.2f, 1105.0f, 320.5f};
    snap.wheels[1] = {13.5f, 82.2f, 1155.0f, 325.5f};
    snap.wheels[2] = {22.5f, 79.2f, -150.0f, 290.5f};
    snap.wheels[3] = {23.5f, 79.8f, 920.0f, 295.5f};

    snap.imu = {1.25f, -2.50f, 9.81f, 0x0105u, 11.1f, -22.2f, 33.3f};

    snap.ecu.tps_pct = 75.5f;
    snap.ecu.map_bar = 1.015f;
    snap.ecu.air_temp_c = 31.2f;
    snap.ecu.engine_temp_c = 89.4f;
    snap.ecu.oil_pressure_bar = 4.25f;
    snap.ecu.fuel_pressure_bar = 3.12f;
    snap.ecu.water_pressure_bar = 1.15f;
    snap.ecu.exhaust_o2_lambda = 0.885f;
    snap.ecu.oil_temp_c = 104.5f;
    snap.ecu.rpm = 10250u;
    snap.ecu.gear = -1; // Ré
    snap.ecu.pit_limit_status = 1u;

    snap.gps_latitude_deg = -27.5948034;
    snap.gps_longitude_deg = -48.5482197;
    snap.gps_altitude_m = 14.25f;
    snap.gps_speed_kmh = 83.4f;
    snap.gps_speed_mps = 23.1667f;
    snap.gps_course_deg = 271.5f;

    rec.fill(0xAAu);
    ASSERT_EQ(bsw::LoggerService::encode_telemetry_record(snap, rec.data(), rec.size()),
              bsw::LoggerService::kRecordSize);

    EXPECT_EQ(rec[0], 0x5Au);
    EXPECT_EQ(rec[1], 0xA5u);
    EXPECT_EQ(read_u16_le(rec.data(), 0u), 0xA55Au);
    EXPECT_EQ(rec[2], 0x01u);
    EXPECT_EQ(rec[3], 0x03u);
    EXPECT_EQ(read_u32_le(rec.data(), 4u), 4242u);
    EXPECT_EQ(read_u64_le(rec.data(), 8u), 987654321ULL);
    EXPECT_EQ(read_u64_le(rec.data(), 16u), 1795098605432ULL);
    EXPECT_EQ(read_u16_le(rec.data(), 24u), 12u);
    EXPECT_EQ(read_u16_le(rec.data(), 26u), 0x02ABu);
    EXPECT_EQ(rec[28], 0x5Fu);
    EXPECT_EQ(rec[29], 11u);
    EXPECT_EQ(read_u16_le(rec.data(), 30u), 0x0105u);

    EXPECT_DOUBLE_EQ(read_f64_le(rec.data(), 32u), snap.gps_latitude_deg);
    EXPECT_DOUBLE_EQ(read_f64_le(rec.data(), 40u), snap.gps_longitude_deg);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 48u), snap.gps_altitude_m);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 52u), snap.gps_speed_kmh);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 56u), snap.gps_speed_mps);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 60u), snap.gps_course_deg);

    for (std::size_t i = 0u; i < hal::kAdcChannelCount; ++i) {
        EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 64u + i * 4u), snap.adc_physical[i]);
    }

    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 104u), snap.front_controls.steering_angle_deg);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 108u), snap.front_controls.brake_pressure_front_bar);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 112u), snap.front_controls.brake_pressure_rear_bar);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 116u), snap.front_controls.brake_pedal_pos_pct);

    for (std::size_t w = 0u; w < bsw::VehicleCanState::kWheelCount; ++w) {
        const std::size_t base = 120u + w * 16u;
        EXPECT_FLOAT_EQ(read_f32_le(rec.data(), base + 0u), snap.wheels[w].damper_pos_mm);
        EXPECT_FLOAT_EQ(read_f32_le(rec.data(), base + 4u), snap.wheels[w].wheel_speed_kmh);
        EXPECT_FLOAT_EQ(read_f32_le(rec.data(), base + 8u), snap.wheels[w].load_cell_n);
        EXPECT_FLOAT_EQ(read_f32_le(rec.data(), base + 12u), snap.wheels[w].brake_disc_temp_c);
    }

    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 184u), snap.imu.accel_x_mps2);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 188u), snap.imu.accel_y_mps2);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 192u), snap.imu.accel_z_mps2);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 196u), snap.imu.gyro_x_dps);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 200u), snap.imu.gyro_y_dps);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 204u), snap.imu.gyro_z_dps);

    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 208u), snap.ecu.tps_pct);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 212u), snap.ecu.map_bar);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 216u), snap.ecu.air_temp_c);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 220u), snap.ecu.engine_temp_c);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 224u), snap.ecu.oil_pressure_bar);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 228u), snap.ecu.fuel_pressure_bar);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 232u), snap.ecu.water_pressure_bar);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 236u), snap.ecu.exhaust_o2_lambda);
    EXPECT_FLOAT_EQ(read_f32_le(rec.data(), 240u), snap.ecu.oil_temp_c);

    EXPECT_EQ(read_u16_le(rec.data(), 244u), 10250u);
    EXPECT_EQ(read_i16_le(rec.data(), 246u), -1);
    EXPECT_EQ(read_u16_le(rec.data(), 248u), 1u);
    EXPECT_EQ(read_u16_le(rec.data(), 250u), 0u);

    const uint32_t expected_crc = bsw::LoggerService::compute_crc32(rec.data(), 252u);
    EXPECT_EQ(read_u32_le(rec.data(), 252u), expected_crc);

    // Qualquer bit invertido no payload invalida o CRC-32
    rec[150] ^= 0x80u;
    EXPECT_NE(bsw::LoggerService::compute_crc32(rec.data(), 252u), expected_crc);
}

TEST_F(LoggerAndAggregatorTest, StartSessionOpensRtcTimestampedBinFileWrites256ByteHeaderAndSyncs) {
    init_synchronized_rtc(2000ULL);

    bsw::LoggerService logger(mock_storage_hal_, 5u, 100u);
    std::vector<uint8_t> written_data;

    EXPECT_CALL(mock_storage_hal_, mount(_)).WillOnce(Return(true));
    ASSERT_TRUE(logger.initialize());

    EXPECT_CALL(mock_storage_hal_, is_mounted()).WillOnce(Return(true));
    EXPECT_CALL(mock_storage_hal_, get_free_space_bytes(_))
        .WillOnce(DoAll(SetArgReferee<0>(10ULL * 1024ULL * 1024ULL), Return(true)));
    EXPECT_CALL(mock_storage_hal_, open_file(std::string_view("/sdcard/LOG_20261119_143005.bin")))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_storage_hal_, write(_, _))
        .WillOnce(Invoke([&written_data](const char* data, std::size_t len) {
            const auto* bytes = reinterpret_cast<const uint8_t*>(data);
            written_data.insert(written_data.end(), bytes, bytes + len);
            return true;
        }));
    EXPECT_CALL(mock_storage_hal_, sync()).WillOnce(Return(true));

    EXPECT_TRUE(logger.start_session(rtc_service_));
    EXPECT_TRUE(logger.is_session_active());
    EXPECT_EQ(logger.current_file_path(), "/sdcard/LOG_20261119_143005.bin");
    EXPECT_EQ(logger.sync_count(), 1u);
    EXPECT_EQ(logger.bytes_written(), 256ULL);
    ASSERT_EQ(written_data.size(), 256u);

    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(written_data.data()), 4u), "PDLG");
    EXPECT_EQ(read_u16_le(written_data.data(), 4u), 1u);
    EXPECT_EQ(read_u16_le(written_data.data(), 10u), bsw::binlog::kHeaderFlagRtcSynced);
    EXPECT_EQ(read_u16_le(written_data.data(), 12u), 100u);
    EXPECT_EQ(read_u32_le(written_data.data(), 252u),
              bsw::LoggerService::compute_crc32(written_data.data(), 252u));
}

TEST_F(LoggerAndAggregatorTest, StartSessionUsesBootFallbackBinNameWhenRtcDegraded) {
    init_degraded_rtc(4321ULL);

    bsw::LoggerService logger(mock_storage_hal_, 5u);
    std::vector<uint8_t> written_data;

    EXPECT_CALL(mock_storage_hal_, mount(_)).WillOnce(Return(true));
    ASSERT_TRUE(logger.initialize());

    EXPECT_CALL(mock_storage_hal_, is_mounted()).WillOnce(Return(true));
    EXPECT_CALL(mock_storage_hal_, get_free_space_bytes(_))
        .WillOnce(DoAll(SetArgReferee<0>(8ULL * 1024ULL * 1024ULL), Return(true)));
    EXPECT_CALL(mock_storage_hal_, open_file(std::string_view("/sdcard/LOG_BOOT_0000004321ms.bin")))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_storage_hal_, write(_, _))
        .WillOnce(Invoke([&written_data](const char* data, std::size_t len) {
            const auto* bytes = reinterpret_cast<const uint8_t*>(data);
            written_data.insert(written_data.end(), bytes, bytes + len);
            return true;
        }));
    EXPECT_CALL(mock_storage_hal_, sync()).WillOnce(Return(true));

    EXPECT_TRUE(logger.start_session(rtc_service_));
    EXPECT_EQ(logger.current_file_path(), "/sdcard/LOG_BOOT_0000004321ms.bin");
    ASSERT_EQ(written_data.size(), 256u);
    EXPECT_EQ(read_u16_le(written_data.data(), 10u), 0u);
    EXPECT_EQ(read_u64_le(written_data.data(), 16u), 4321ULL);
    EXPECT_EQ(read_u64_le(written_data.data(), 24u), 4321ULL);
}

TEST_F(LoggerAndAggregatorTest, StartSessionFailsWhenFreeSpaceBelowMinimum) {
    init_synchronized_rtc(2000ULL);
    bsw::LoggerService logger(mock_storage_hal_, 5u);

    EXPECT_CALL(mock_storage_hal_, mount(_)).WillOnce(Return(true));
    ASSERT_TRUE(logger.initialize());

    EXPECT_CALL(mock_storage_hal_, is_mounted()).WillOnce(Return(true));
    EXPECT_CALL(mock_storage_hal_, get_free_space_bytes(_))
        .WillOnce(DoAll(SetArgReferee<0>(bsw::LoggerService::kMinFreeSpaceBytes - 1ULL), Return(true)));
    EXPECT_CALL(mock_storage_hal_, open_file(_)).Times(0);

    EXPECT_FALSE(logger.start_session(rtc_service_));
    EXPECT_FALSE(logger.is_session_active());
    EXPECT_EQ(logger.storage_error_count(), 1u);
}

TEST_F(LoggerAndAggregatorTest, ProcessQueueStepBuffersBinaryRecordsSyncsEveryNSamplesAndFlushesOnStop) {
    init_synchronized_rtc(1000ULL);
    bsw::LoggerService logger(mock_storage_hal_, 3u);
    FakeOsalQueue<bsw::TelemetrySnapshot> queue(8u);

    EXPECT_CALL(mock_storage_hal_, mount(_)).WillOnce(Return(true));
    ASSERT_TRUE(logger.initialize());

    EXPECT_CALL(mock_storage_hal_, is_mounted()).WillOnce(Return(true));
    EXPECT_CALL(mock_storage_hal_, get_free_space_bytes(_))
        .WillOnce(DoAll(SetArgReferee<0>(16ULL * 1024ULL * 1024ULL), Return(true)));
    EXPECT_CALL(mock_storage_hal_, open_file(_)).WillOnce(Return(true));

    std::size_t write_calls = 0u;
    std::vector<uint8_t> captured;
    ON_CALL(mock_storage_hal_, write(_, _))
        .WillByDefault(Invoke([&write_calls, &captured](const char* data, std::size_t len) {
            ++write_calls;
            const auto* bytes = reinterpret_cast<const uint8_t*>(data);
            captured.insert(captured.end(), bytes, bytes + len);
            return true;
        }));
    EXPECT_CALL(mock_storage_hal_, write(_, _)).Times(3); // 1 header (256 B) + lote 3 reg (768 B) + 1 reg no stop (256 B)
    EXPECT_CALL(mock_storage_hal_, sync()).Times(3).WillRepeatedly(Return(true));
    EXPECT_CALL(mock_storage_hal_, close_file()).WillOnce(Return(true));

    ASSERT_TRUE(logger.start_session(rtc_service_));

    // Fila vazia retorna false sem contabilizar erro
    EXPECT_FALSE(logger.process_queue_step(queue, 0u));
    EXPECT_EQ(logger.storage_error_count(), 0u);

    for (uint32_t seq = 1u; seq <= 4u; ++seq) {
        bsw::TelemetrySnapshot snap{};
        snap.monotonic_ms = 1000ULL + seq;
        snap.sample_seq = seq;
        snap.ecu.rpm = static_cast<uint16_t>(4000u + seq * 100u);
        ASSERT_TRUE(queue.send(snap, 0u));
    }

    EXPECT_TRUE(logger.process_queue_step(queue, 0u));
    EXPECT_TRUE(logger.process_queue_step(queue, 0u));
    EXPECT_EQ(logger.pending_block_bytes(), 512u);
    EXPECT_EQ(write_calls, 1u);

    EXPECT_TRUE(logger.process_queue_step(queue, 0u)); // 3ª amostra dispara flush + sync
    EXPECT_EQ(logger.pending_block_bytes(), 0u);
    EXPECT_EQ(logger.samples_logged(), 3u);
    EXPECT_EQ(write_calls, 2u);

    EXPECT_TRUE(logger.process_queue_step(queue, 0u)); // 4ª amostra fica pendente até stop_session()
    EXPECT_EQ(logger.pending_block_bytes(), 256u);

    EXPECT_TRUE(logger.stop_session());
    EXPECT_FALSE(logger.is_session_active());
    EXPECT_EQ(logger.samples_logged(), 4u);
    EXPECT_EQ(logger.sync_count(), 3u);
    EXPECT_EQ(write_calls, 3u);
    EXPECT_EQ(logger.bytes_written(), 1280ULL);
    ASSERT_EQ(captured.size(), 1280u);

    // Valida cabeçalho e todos os 4 registros binários em ordem
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(captured.data()), 4u), "PDLG");
    for (uint32_t seq = 1u; seq <= 4u; ++seq) {
        const uint8_t* rec = captured.data() + seq * 256u;
        EXPECT_EQ(read_u16_le(rec, 0u), 0xA55Au);
        EXPECT_EQ(rec[2], 0x01u);
        EXPECT_EQ(read_u32_le(rec, 4u), seq);
        EXPECT_EQ(read_u64_le(rec, 8u), 1000ULL + seq);
        EXPECT_EQ(read_u16_le(rec, 244u), static_cast<uint16_t>(4000u + seq * 100u));
        EXPECT_EQ(read_u32_le(rec, 252u), bsw::LoggerService::compute_crc32(rec, 252u));
    }
}

TEST_F(LoggerAndAggregatorTest, LogSnapshotAutoFlushesExactlyAfter16RecordsFill4KbBlock) {
    init_synchronized_rtc(1000ULL);
    bsw::LoggerService logger(mock_storage_hal_, 100u); // sync somente após 100 amostras

    EXPECT_CALL(mock_storage_hal_, mount(_)).WillOnce(Return(true));
    ASSERT_TRUE(logger.initialize());

    EXPECT_CALL(mock_storage_hal_, is_mounted()).WillOnce(Return(true));
    EXPECT_CALL(mock_storage_hal_, get_free_space_bytes(_))
        .WillOnce(DoAll(SetArgReferee<0>(16ULL * 1024ULL * 1024ULL), Return(true)));
    EXPECT_CALL(mock_storage_hal_, open_file(_)).WillOnce(Return(true));

    std::vector<std::size_t> write_sizes;
    EXPECT_CALL(mock_storage_hal_, write(_, _))
        .WillRepeatedly(Invoke([&write_sizes](const char*, std::size_t len) {
            write_sizes.push_back(len);
            return true;
        }));
    EXPECT_CALL(mock_storage_hal_, sync()).Times(2).WillRepeatedly(Return(true)); // apenas start + stop
    EXPECT_CALL(mock_storage_hal_, close_file()).WillOnce(Return(true));

    ASSERT_TRUE(logger.start_session(rtc_service_));

    // 16 registros × 256 bytes = 4096 bytes exatos no buffer
    for (uint32_t i = 1u; i <= 16u; ++i) {
        bsw::TelemetrySnapshot snap{};
        snap.monotonic_ms = 2000ULL + i;
        snap.sample_seq = i;
        ASSERT_TRUE(logger.log_snapshot(snap));
    }
    EXPECT_EQ(logger.pending_block_bytes(), 4096u);
    ASSERT_EQ(write_sizes.size(), 1u); // Apenas o header (256 B) foi escrito até aqui

    // 17º registro força flush exato do bloco de 4096 bytes e retém 256 bytes no buffer
    bsw::TelemetrySnapshot snap17{};
    snap17.monotonic_ms = 2017ULL;
    snap17.sample_seq = 17u;
    ASSERT_TRUE(logger.log_snapshot(snap17));
    EXPECT_EQ(logger.pending_block_bytes(), 256u);
    ASSERT_EQ(write_sizes.size(), 2u);
    EXPECT_EQ(write_sizes[1], 4096u);

    EXPECT_TRUE(logger.stop_session());
    ASSERT_EQ(write_sizes.size(), 3u);
    EXPECT_EQ(write_sizes[2], 256u);
    EXPECT_EQ(logger.bytes_written(), 256ULL + 17ULL * 256ULL);
}

TEST_F(LoggerAndAggregatorTest, LogSnapshotCountsStorageErrorOnWriteOrSyncFailure) {
    init_synchronized_rtc(1000ULL);
    bsw::LoggerService logger(mock_storage_hal_, 1u);

    EXPECT_CALL(mock_storage_hal_, mount(_)).WillOnce(Return(true));
    ASSERT_TRUE(logger.initialize());

    EXPECT_CALL(mock_storage_hal_, is_mounted()).WillOnce(Return(true));
    EXPECT_CALL(mock_storage_hal_, get_free_space_bytes(_))
        .WillOnce(DoAll(SetArgReferee<0>(16ULL * 1024ULL * 1024ULL), Return(true)));
    EXPECT_CALL(mock_storage_hal_, open_file(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_storage_hal_, write(_, _))
        .WillOnce(Return(true))
        .WillOnce(Return(false));
    EXPECT_CALL(mock_storage_hal_, sync()).WillOnce(Return(true));

    ASSERT_TRUE(logger.start_session(rtc_service_));

    bsw::TelemetrySnapshot snap{};
    snap.monotonic_ms = 1001ULL;
    EXPECT_FALSE(logger.log_snapshot(snap));
    EXPECT_EQ(logger.storage_error_count(), 1u);
}

TEST_F(LoggerAndAggregatorTest, EndToEndBinaryStreamSupportsCrashTailResyncRecovery) {
    std::vector<uint8_t> file_bytes;

    std::array<uint8_t, bsw::LoggerService::kFileHeaderSize> hdr{};
    ASSERT_EQ(bsw::LoggerService::encode_file_header(
                  {1000ULL, 1795098600000ULL, 100u, true, 3600u}, hdr.data(), hdr.size()),
              256u);
    file_bytes.insert(file_bytes.end(), hdr.begin(), hdr.end());

    // Registro #1 íntegro
    bsw::TelemetrySnapshot s1{};
    s1.sample_seq = 10u;
    s1.ecu.rpm = 6500u;
    std::array<uint8_t, bsw::LoggerService::kRecordSize> r1{};
    ASSERT_EQ(bsw::LoggerService::encode_telemetry_record(s1, r1.data(), r1.size()), 256u);
    file_bytes.insert(file_bytes.end(), r1.begin(), r1.end());

    // 7 bytes de lixo no meio simulando setor parcialmente corrompido
    const std::array<uint8_t, 7> junk{0xFFu, 0x00u, 0x5Au, 0x11u, 0x22u, 0x33u, 0x44u};
    file_bytes.insert(file_bytes.end(), junk.begin(), junk.end());

    // Registro #2 íntegro
    bsw::TelemetrySnapshot s2{};
    s2.sample_seq = 11u;
    s2.ecu.rpm = 7200u;
    std::array<uint8_t, bsw::LoggerService::kRecordSize> r2{};
    ASSERT_EQ(bsw::LoggerService::encode_telemetry_record(s2, r2.data(), r2.size()), 256u);
    file_bytes.insert(file_bytes.end(), r2.begin(), r2.end());

    // Registro #3 truncado pela metade (corte de energia durante escrita no SD)
    bsw::TelemetrySnapshot s3{};
    s3.sample_seq = 12u;
    std::array<uint8_t, bsw::LoggerService::kRecordSize> r3{};
    ASSERT_EQ(bsw::LoggerService::encode_telemetry_record(s3, r3.data(), r3.size()), 256u);
    file_bytes.insert(file_bytes.end(), r3.begin(), r3.begin() + 100);

    // Algoritmo de resync especificado em docs/BINARY_LOG_FORMAT.md
    ASSERT_GE(file_bytes.size(), 256u);
    ASSERT_EQ(read_u32_le(file_bytes.data(), 252u),
              bsw::LoggerService::compute_crc32(file_bytes.data(), 252u));

    std::vector<uint32_t> recovered_seqs;
    std::size_t cursor = read_u16_le(file_bytes.data(), 6u); // header_size = 256
    const std::size_t rec_size = read_u16_le(file_bytes.data(), 8u); // record_size = 256

    while (cursor + rec_size <= file_bytes.size()) {
        const uint8_t* ptr = file_bytes.data() + cursor;
        if (ptr[0] == 0x5Au && ptr[1] == 0xA5u && ptr[2] == 0x01u) {
            const uint32_t stored_crc = read_u32_le(ptr, rec_size - 4u);
            if (stored_crc == bsw::LoggerService::compute_crc32(ptr, rec_size - 4u)) {
                recovered_seqs.push_back(read_u32_le(ptr, 4u));
                cursor += rec_size;
                continue;
            }
        }
        ++cursor; // Avança 1 byte para ressincronizar no próximo Sync Word válido
    }

    ASSERT_EQ(recovered_seqs.size(), 2u);
    EXPECT_EQ(recovered_seqs[0], 10u);
    EXPECT_EQ(recovered_seqs[1], 11u);
}

TEST_F(LoggerAndAggregatorTest, SdCardLldValidatesPinsAndHandlesMountUnmountLifecycle) {
    lld::SdCardLld sd_lld;

    // Pino reservado da Flash SPI (GPIO 26) deve ser rejeitado
    hal::StorageConfig flash_pin_cfg{};
    flash_pin_cfg.clk_pin = GPIO_NUM_26;
    EXPECT_FALSE(lld::SdCardLld::is_valid_config(flash_pin_cfg));
    EXPECT_FALSE(sd_lld.mount(flash_pin_cfg));
    EXPECT_FALSE(sd_lld.is_mounted());

    // Pino duplicado deve ser rejeitado
    hal::StorageConfig dup_pin_cfg{};
    dup_pin_cfg.d1_pin = dup_pin_cfg.clk_pin;
    EXPECT_FALSE(lld::SdCardLld::is_valid_config(dup_pin_cfg));
    EXPECT_FALSE(sd_lld.mount(dup_pin_cfg));

    // Configuração oficial BSP (SDMMC 4-bit: GPIO 14, 17, 18, 21, 40, 41)
    hal::StorageConfig valid_sdmmc{};
    EXPECT_TRUE(lld::SdCardLld::is_valid_config(valid_sdmmc));
    EXPECT_TRUE(sd_lld.mount(valid_sdmmc));
    EXPECT_TRUE(sd_lld.is_mounted());

    uint64_t free_bytes = 0ULL;
    EXPECT_TRUE(sd_lld.get_free_space_bytes(free_bytes));
    EXPECT_GT(free_bytes, 0ULL);

    // Teste real de abertura/escrita binária/sync/fechamento em arquivo temporário no Host
    const auto temp_path = (std::filesystem::temp_directory_path() / "datalogger_sd_lld_sil_test.bin").string();
    std::filesystem::remove(temp_path);
    ASSERT_TRUE(sd_lld.open_file(temp_path));
    const std::array<uint8_t, 8> bin_payload{'P', 'D', 'L', 'G', 0x01u, 0x00u, 0x5Au, 0xA5u};
    EXPECT_TRUE(sd_lld.write(reinterpret_cast<const char*>(bin_payload.data()), bin_payload.size()));
    EXPECT_TRUE(sd_lld.sync());
    EXPECT_TRUE(sd_lld.close_file());
    EXPECT_TRUE(std::filesystem::exists(temp_path));
    EXPECT_EQ(std::filesystem::file_size(temp_path), 8u);
    std::filesystem::remove(temp_path);

    EXPECT_TRUE(sd_lld.unmount());
    EXPECT_FALSE(sd_lld.is_mounted());

    // Modo SPI com pinos do BSP
    hal::StorageConfig valid_spi{};
    valid_spi.bus_mode = hal::SdBusMode::kSpi;
    EXPECT_TRUE(lld::SdCardLld::is_valid_config(valid_spi));
    EXPECT_TRUE(sd_lld.mount(valid_spi));
    EXPECT_TRUE(sd_lld.unmount());
}

} // namespace datalogger::sil
