#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "ds3231_i2c_lld.hpp"
#include "mock_rtc_hal.hpp"
#include "rtc_service.hpp"

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::SetArgReferee;

namespace datalogger::sil {

namespace {

hal::RtcDateTime make_valid_dt(uint16_t year = 2026,
                               uint8_t month = 11,
                               uint8_t day = 19,
                               uint8_t dow = 4,
                               uint8_t hour = 18,
                               uint8_t minute = 39,
                               uint8_t second = 10) {
    hal::RtcDateTime dt{};
    dt.year = year;
    dt.month = month;
    dt.day = day;
    dt.day_of_week = dow;
    dt.hour = hour;
    dt.minute = minute;
    dt.second = second;
    return dt;
}

} // namespace

class RtcServiceTest : public ::testing::Test {
protected:
    NiceMock<MockRtcHal> mock_rtc_hal_;
    bsw::RtcService service_{mock_rtc_hal_};
};

TEST_F(RtcServiceTest, DefaultConfigUsesBspI2cPinsAnd400Khz) {
    const hal::RtcI2cConfig cfg{};
    EXPECT_EQ(cfg.sda_pin, PIN_I2C_SDA);
    EXPECT_EQ(cfg.scl_pin, PIN_I2C_SCL);
    EXPECT_EQ(cfg.sda_pin, GPIO_NUM_11);
    EXPECT_EQ(cfg.scl_pin, GPIO_NUM_12);
    EXPECT_EQ(cfg.clk_speed_hz, static_cast<uint32_t>(I2C_MASTER_FREQ_HZ));
    EXPECT_EQ(cfg.clk_speed_hz, 400000u);
    EXPECT_EQ(cfg.device_address, static_cast<uint8_t>(RTC_DS3231_I2C_ADDR));
    EXPECT_EQ(cfg.device_address, 0x68u);
    EXPECT_GT(cfg.timeout_ms, 0);
}

TEST_F(RtcServiceTest, InitialStateIsUninitializedAndNotSynchronized) {
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kUninitialized);
    EXPECT_FALSE(service_.is_synchronized());
    EXPECT_FALSE(service_.is_degraded());
    EXPECT_EQ(service_.sync_count(), 0u);
    EXPECT_EQ(service_.sync_error_count(), 0u);
    EXPECT_EQ(service_.anchor_monotonic_ms(), 0ULL);
    EXPECT_EQ(service_.anchor_epoch_ms(), 0ULL);
}

TEST_F(RtcServiceTest, InitializeSyncsClockWithMidpointAnchorAnd1MsResolution) {
    const hal::RtcDateTime rtc_dt = make_valid_dt(2026, 11, 19, 4, 18, 39, 10);
    const hal::RtcDeviceStatus ok_status{false};

    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(true));
    {
        Sequence seq;
        EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms()).InSequence(seq).WillOnce(Return(1000ULL));
        EXPECT_CALL(mock_rtc_hal_, read_datetime(_, _))
            .InSequence(seq)
            .WillOnce(DoAll(SetArgReferee<0>(rtc_dt), SetArgReferee<1>(ok_status), Return(true)));
        EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms()).InSequence(seq).WillOnce(Return(1002ULL));
    }

    ASSERT_TRUE(service_.initialize());
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kSynchronized);
    EXPECT_TRUE(service_.is_synchronized());
    EXPECT_FALSE(service_.is_degraded());
    EXPECT_EQ(service_.sync_count(), 1u);
    EXPECT_EQ(service_.sync_error_count(), 0u);
    EXPECT_EQ(service_.anchor_monotonic_ms(), 1001ULL);

    const uint64_t expected_base_epoch = bsw::RtcService::datetime_to_epoch_ms(rtc_dt);
    EXPECT_EQ(service_.anchor_epoch_ms(), expected_base_epoch);

    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms()).WillOnce(Return(1002ULL));
    const bsw::RtcTimestamp ts1 = service_.sample_timestamp();
    EXPECT_TRUE(ts1.synchronized);
    EXPECT_EQ(ts1.monotonic_ms, 1002ULL);
    EXPECT_EQ(ts1.epoch_ms, expected_base_epoch + 1ULL);

    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms()).WillOnce(Return(1051ULL));
    EXPECT_EQ(service_.now_epoch_ms(), expected_base_epoch + 50ULL);

    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms()).WillOnce(Return(61001ULL));
    hal::RtcDateTime now_dt{};
    ASSERT_TRUE(service_.now_datetime(now_dt));
    EXPECT_EQ(now_dt.year, 2026u);
    EXPECT_EQ(now_dt.month, 11u);
    EXPECT_EQ(now_dt.day, 19u);
    EXPECT_EQ(now_dt.day_of_week, 4u);
    EXPECT_EQ(now_dt.hour, 18u);
    EXPECT_EQ(now_dt.minute, 40u);
    EXPECT_EQ(now_dt.second, 10u);
}

TEST_F(RtcServiceTest, SubSecondAlignmentWithin50MsTarget1Ms) {
    const hal::RtcDateTime rtc_dt = make_valid_dt(2026, 11, 19, 4, 18, 39, 10);
    const hal::RtcDeviceStatus ok_status{false};

    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms())
        .WillOnce(Return(5000ULL))
        .WillOnce(Return(5004ULL))
        .WillOnce(Return(5010ULL))
        .WillOnce(Return(5011ULL));
    EXPECT_CALL(mock_rtc_hal_, read_datetime(_, _))
        .WillOnce(DoAll(SetArgReferee<0>(rtc_dt), SetArgReferee<1>(ok_status), Return(true)));

    ASSERT_TRUE(service_.initialize());
    EXPECT_EQ(service_.anchor_monotonic_ms(), 5002ULL);

    const bsw::RtcTimestamp s1 = service_.sample_timestamp();
    const bsw::RtcTimestamp s2 = service_.sample_timestamp();
    EXPECT_EQ(s2.monotonic_ms - s1.monotonic_ms, 1ULL);
    EXPECT_EQ(s2.epoch_ms - s1.epoch_ms, 1ULL);
}

TEST_F(RtcServiceTest, ResyncUpdatesAnchorAndHandlesTemporaryFailureAndRecovery) {
    const hal::RtcDateTime dt1 = make_valid_dt(2026, 11, 19, 4, 18, 0, 0);
    const hal::RtcDateTime dt2 = make_valid_dt(2026, 11, 19, 4, 19, 0, 0);
    const hal::RtcDeviceStatus ok_status{false};

    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms())
        .WillOnce(Return(100ULL))
        .WillOnce(Return(102ULL))
        .WillOnce(Return(60100ULL))
        .WillOnce(Return(60104ULL))
        .WillOnce(Return(70000ULL))
        .WillOnce(Return(70002ULL));
    EXPECT_CALL(mock_rtc_hal_, read_datetime(_, _))
        .WillOnce(DoAll(SetArgReferee<0>(dt1), SetArgReferee<1>(ok_status), Return(true)))
        .WillOnce(Return(false))
        .WillOnce(DoAll(SetArgReferee<0>(dt2), SetArgReferee<1>(ok_status), Return(true)));

    ASSERT_TRUE(service_.initialize());
    EXPECT_TRUE(service_.is_synchronized());
    EXPECT_EQ(service_.sync_count(), 1u);

    EXPECT_FALSE(service_.resync());
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kI2cReadFailed);
    EXPECT_FALSE(service_.is_synchronized());
    EXPECT_TRUE(service_.is_degraded());
    EXPECT_EQ(service_.sync_error_count(), 1u);

    ASSERT_TRUE(service_.resync());
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kSynchronized);
    EXPECT_TRUE(service_.is_synchronized());
    EXPECT_FALSE(service_.is_degraded());
    EXPECT_EQ(service_.sync_count(), 2u);
    EXPECT_EQ(service_.anchor_monotonic_ms(), 70001ULL);
    EXPECT_EQ(service_.anchor_epoch_ms(), bsw::RtcService::datetime_to_epoch_ms(dt2));
}

TEST_F(RtcServiceTest, ResyncBeforeInitializeFailsSafely) {
    EXPECT_CALL(mock_rtc_hal_, read_datetime(_, _)).Times(0);

    EXPECT_FALSE(service_.resync());
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kHalInitFailed);
    EXPECT_FALSE(service_.is_synchronized());
    EXPECT_TRUE(service_.is_degraded());
    EXPECT_EQ(service_.sync_error_count(), 1u);
}

TEST_F(RtcServiceTest, ResyncTransitionsFromSynchronizedToDegradedOnOsfOrInvalidCalendar) {
    const hal::RtcDateTime valid_dt = make_valid_dt(2026, 11, 19, 4, 18, 0, 0);
    const hal::RtcDateTime invalid_dt = make_valid_dt(2025, 2, 29, 6, 12, 0, 0);
    const hal::RtcDeviceStatus ok_status{false};
    const hal::RtcDeviceStatus osf_status{true};

    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms()).WillRepeatedly(Return(1000ULL));
    EXPECT_CALL(mock_rtc_hal_, read_datetime(_, _))
        .WillOnce(DoAll(SetArgReferee<0>(valid_dt), SetArgReferee<1>(ok_status), Return(true)))
        .WillOnce(DoAll(SetArgReferee<0>(valid_dt), SetArgReferee<1>(osf_status), Return(true)))
        .WillOnce(DoAll(SetArgReferee<0>(invalid_dt), SetArgReferee<1>(ok_status), Return(true)));

    ASSERT_TRUE(service_.initialize());
    EXPECT_TRUE(service_.is_synchronized());

    EXPECT_FALSE(service_.resync());
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kOscillatorStopped);
    EXPECT_FALSE(service_.is_synchronized());
    EXPECT_TRUE(service_.is_degraded());

    EXPECT_FALSE(service_.resync());
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kInvalidCalendar);
    EXPECT_FALSE(service_.is_synchronized());
    EXPECT_TRUE(service_.is_degraded());
}

TEST_F(RtcServiceTest, SetDatetimeWritesHalAndSynchronizesFromDegradedState) {
    const hal::RtcDateTime default_dt = make_valid_dt(2000, 1, 1, 6, 0, 0, 0);
    const hal::RtcDeviceStatus osf_status{true};
    const hal::RtcDateTime target_dt = make_valid_dt(2026, 11, 19, 4, 18, 45, 0);

    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_rtc_hal_, read_datetime(_, _))
        .WillOnce(DoAll(SetArgReferee<0>(default_dt), SetArgReferee<1>(osf_status), Return(true)));
    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms())
        .WillOnce(Return(100ULL))
        .WillOnce(Return(102ULL))
        .WillOnce(Return(2000ULL))
        .WillOnce(Return(2004ULL));
    EXPECT_CALL(mock_rtc_hal_, write_datetime(_)).WillOnce(Return(true));

    EXPECT_FALSE(service_.initialize());
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kOscillatorStopped);
    EXPECT_TRUE(service_.is_degraded());

    ASSERT_TRUE(service_.set_datetime(target_dt));
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kSynchronized);
    EXPECT_TRUE(service_.is_synchronized());
    EXPECT_FALSE(service_.is_degraded());
    EXPECT_EQ(service_.sync_count(), 1u);
    EXPECT_EQ(service_.anchor_monotonic_ms(), 2002ULL);
    EXPECT_EQ(service_.anchor_epoch_ms(), bsw::RtcService::datetime_to_epoch_ms(target_dt));
}

TEST_F(RtcServiceTest, SetDatetimeBeforeInitializeOrAfterHalInitFailedRejectsWithoutCallingHal) {
    const hal::RtcDateTime target_dt = make_valid_dt();
    EXPECT_CALL(mock_rtc_hal_, write_datetime(_)).Times(0);

    EXPECT_FALSE(service_.set_datetime(target_dt));
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kUninitialized);
    EXPECT_EQ(service_.sync_error_count(), 1u);

    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(false));
    EXPECT_FALSE(service_.initialize());
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kHalInitFailed);

    EXPECT_FALSE(service_.set_datetime(target_dt));
    EXPECT_FALSE(service_.is_synchronized());
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kHalInitFailed);
    EXPECT_EQ(service_.sync_error_count(), 2u);
}

TEST_F(RtcServiceTest, SetDatetimeRejectsInvalidCalendarWithoutCallingHalAndPreservesSyncState) {
    const hal::RtcDateTime valid_dt = make_valid_dt(2026, 11, 19, 4, 18, 0, 0);
    const hal::RtcDeviceStatus ok_status{false};

    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms()).WillRepeatedly(Return(1000ULL));
    EXPECT_CALL(mock_rtc_hal_, read_datetime(_, _))
        .WillOnce(DoAll(SetArgReferee<0>(valid_dt), SetArgReferee<1>(ok_status), Return(true)));
    EXPECT_CALL(mock_rtc_hal_, write_datetime(_)).Times(0);

    ASSERT_TRUE(service_.initialize());
    const uint64_t expected_epoch = service_.anchor_epoch_ms();

    const hal::RtcDateTime bad_leap = make_valid_dt(2025, 2, 29, 6, 12, 0, 0);
    EXPECT_FALSE(service_.set_datetime(bad_leap));
    EXPECT_TRUE(service_.is_synchronized());
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kSynchronized);
    EXPECT_EQ(service_.anchor_epoch_ms(), expected_epoch);
    EXPECT_EQ(service_.sync_error_count(), 1u);
}

TEST_F(RtcServiceTest, SetDatetimeHandlesHalWriteFailureWithoutCorruptingState) {
    const hal::RtcDateTime valid_dt = make_valid_dt();

    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_rtc_hal_, read_datetime(_, _)).WillOnce(Return(false));
    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms()).WillRepeatedly(Return(500ULL));
    EXPECT_CALL(mock_rtc_hal_, write_datetime(_)).WillOnce(Return(false));

    EXPECT_FALSE(service_.initialize());
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kI2cReadFailed);

    EXPECT_FALSE(service_.set_datetime(valid_dt));
    EXPECT_FALSE(service_.is_synchronized());
    EXPECT_TRUE(service_.is_degraded());
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kI2cReadFailed);
    EXPECT_EQ(service_.sync_error_count(), 2u);
}

TEST_F(RtcServiceTest, DegradedFallbackWhenHalInitFails) {
    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(false));
    EXPECT_CALL(mock_rtc_hal_, read_datetime(_, _)).Times(0);
    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms()).WillRepeatedly(Return(5432ULL));

    EXPECT_FALSE(service_.initialize());
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kHalInitFailed);
    EXPECT_FALSE(service_.is_synchronized());
    EXPECT_TRUE(service_.is_degraded());
    EXPECT_EQ(service_.sync_error_count(), 1u);
    EXPECT_EQ(service_.now_epoch_ms(), 5432ULL);

    const bsw::RtcTimestamp ts = service_.sample_timestamp();
    EXPECT_FALSE(ts.synchronized);
    EXPECT_EQ(ts.monotonic_ms, 5432ULL);
    EXPECT_EQ(ts.epoch_ms, 5432ULL);

    hal::RtcDateTime out_dt{};
    EXPECT_FALSE(service_.now_datetime(out_dt));
}

TEST_F(RtcServiceTest, DegradedFallbackWhenI2cReadFails) {
    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_rtc_hal_, read_datetime(_, _)).WillOnce(Return(false));
    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms()).WillRepeatedly(Return(9876ULL));

    EXPECT_FALSE(service_.initialize());
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kI2cReadFailed);
    EXPECT_FALSE(service_.is_synchronized());
    EXPECT_TRUE(service_.is_degraded());
    EXPECT_EQ(service_.sync_error_count(), 1u);

    const bsw::RtcTimestamp ts = service_.sample_timestamp();
    EXPECT_FALSE(ts.synchronized);
    EXPECT_EQ(ts.monotonic_ms, 9876ULL);
    EXPECT_EQ(ts.epoch_ms, 9876ULL);
}

TEST_F(RtcServiceTest, DegradedFallbackWhenOscillatorStoppedFlagSet) {
    const hal::RtcDateTime valid_default_dt = make_valid_dt(2000, 1, 1, 6, 0, 0, 0);
    const hal::RtcDeviceStatus osf_status{true};

    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_rtc_hal_, read_datetime(_, _))
        .WillOnce(DoAll(SetArgReferee<0>(valid_default_dt), SetArgReferee<1>(osf_status), Return(true)));
    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms()).WillRepeatedly(Return(3210ULL));

    EXPECT_FALSE(service_.initialize());
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kOscillatorStopped);
    EXPECT_FALSE(service_.is_synchronized());
    EXPECT_TRUE(service_.is_degraded());
    EXPECT_EQ(service_.now_epoch_ms(), 3210ULL);

    hal::RtcDateTime out_dt{};
    EXPECT_FALSE(service_.now_datetime(out_dt));
}

TEST_F(RtcServiceTest, DegradedFallbackWhenRtcReturnsInvalidCalendar) {
    const hal::RtcDateTime invalid_dt = make_valid_dt(2025, 2, 29, 6, 10, 0, 0);
    const hal::RtcDeviceStatus ok_status{false};

    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_rtc_hal_, read_datetime(_, _))
        .WillOnce(DoAll(SetArgReferee<0>(invalid_dt), SetArgReferee<1>(ok_status), Return(true)));
    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms()).WillRepeatedly(Return(4444ULL));

    EXPECT_FALSE(service_.initialize());
    EXPECT_EQ(service_.status(), bsw::RtcSyncStatus::kInvalidCalendar);
    EXPECT_FALSE(service_.is_synchronized());
    EXPECT_TRUE(service_.is_degraded());
    EXPECT_EQ(service_.now_epoch_ms(), 4444ULL);
}

TEST_F(RtcServiceTest, MonotonicToEpochMsHandlesTimestampsBeforeAndAfterAnchorWithoutUnderflow) {
    const hal::RtcDateTime dt = make_valid_dt(2026, 11, 19, 4, 18, 39, 10);
    const hal::RtcDeviceStatus ok_status{false};

    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms())
        .WillOnce(Return(100ULL))
        .WillOnce(Return(100ULL));
    EXPECT_CALL(mock_rtc_hal_, read_datetime(_, _))
        .WillOnce(DoAll(SetArgReferee<0>(dt), SetArgReferee<1>(ok_status), Return(true)));

    ASSERT_TRUE(service_.initialize());
    const uint64_t anchor_epoch = service_.anchor_epoch_ms();

    EXPECT_EQ(service_.monotonic_to_epoch_ms(100ULL), anchor_epoch);
    EXPECT_EQ(service_.monotonic_to_epoch_ms(150ULL), anchor_epoch + 50ULL);
    EXPECT_EQ(service_.monotonic_to_epoch_ms(75ULL), anchor_epoch - 25ULL);
    EXPECT_EQ(service_.monotonic_to_epoch_ms(0ULL), anchor_epoch - 100ULL);
}

TEST_F(RtcServiceTest, MonotonicToEpochMsReturnsRawMonotonicWhenDegraded) {
    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(false));
    EXPECT_FALSE(service_.initialize());

    EXPECT_EQ(service_.monotonic_to_epoch_ms(12345ULL), 12345ULL);
}

TEST_F(RtcServiceTest, NowDatetimeReturnsFalseWhenDegradedAndTrueWhenSynchronized) {
    hal::RtcDateTime out_dt{};
    EXPECT_FALSE(service_.now_datetime(out_dt));

    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_rtc_hal_, read_datetime(_, _)).WillOnce(Return(false));
    EXPECT_FALSE(service_.initialize());
    EXPECT_FALSE(service_.now_datetime(out_dt));

    const hal::RtcDateTime target_dt = make_valid_dt(2026, 11, 19, 4, 18, 39, 10);
    EXPECT_CALL(mock_rtc_hal_, write_datetime(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms()).WillRepeatedly(Return(1000ULL));
    ASSERT_TRUE(service_.set_datetime(target_dt));
    ASSERT_TRUE(service_.now_datetime(out_dt));
    EXPECT_EQ(out_dt.year, 2026u);
    EXPECT_EQ(out_dt.month, 11u);
    EXPECT_EQ(out_dt.day, 19u);
    EXPECT_EQ(out_dt.hour, 18u);
    EXPECT_EQ(out_dt.minute, 39u);
    EXPECT_EQ(out_dt.second, 10u);
}

TEST_F(RtcServiceTest, CalendarValidationEnforcesYear2000To2099AndLeapYears) {
    EXPECT_TRUE(bsw::RtcService::is_leap_year(2000u));
    EXPECT_TRUE(bsw::RtcService::is_leap_year(2024u));
    EXPECT_FALSE(bsw::RtcService::is_leap_year(2025u));
    EXPECT_FALSE(bsw::RtcService::is_leap_year(1900u));
    EXPECT_FALSE(bsw::RtcService::is_leap_year(2100u));

    EXPECT_EQ(bsw::RtcService::days_in_month(2024u, 2u), 29u);
    EXPECT_EQ(bsw::RtcService::days_in_month(2025u, 2u), 28u);
    EXPECT_EQ(bsw::RtcService::days_in_month(2026u, 4u), 30u);
    EXPECT_EQ(bsw::RtcService::days_in_month(2026u, 12u), 31u);
    EXPECT_EQ(bsw::RtcService::days_in_month(2026u, 0u), 0u);
    EXPECT_EQ(bsw::RtcService::days_in_month(2026u, 13u), 0u);

    EXPECT_TRUE(bsw::RtcService::is_valid_datetime(make_valid_dt(2000, 1, 1, 6, 0, 0, 0)));
    EXPECT_TRUE(bsw::RtcService::is_valid_datetime(make_valid_dt(2000, 2, 29, 2, 23, 59, 59)));
    EXPECT_TRUE(bsw::RtcService::is_valid_datetime(make_valid_dt(2024, 2, 29, 4, 12, 0, 0)));
    EXPECT_TRUE(bsw::RtcService::is_valid_datetime(make_valid_dt(2099, 12, 31, 4, 23, 59, 59)));

    EXPECT_FALSE(bsw::RtcService::is_valid_datetime(make_valid_dt(1999, 12, 31, 5, 0, 0, 0)));
    EXPECT_FALSE(bsw::RtcService::is_valid_datetime(make_valid_dt(2100, 1, 1, 5, 0, 0, 0)));
    EXPECT_FALSE(bsw::RtcService::is_valid_datetime(make_valid_dt(2025, 2, 29, 6, 0, 0, 0)));
    EXPECT_FALSE(bsw::RtcService::is_valid_datetime(make_valid_dt(2026, 4, 31, 5, 0, 0, 0)));
    EXPECT_FALSE(bsw::RtcService::is_valid_datetime(make_valid_dt(2026, 0, 10, 1, 0, 0, 0)));
    EXPECT_FALSE(bsw::RtcService::is_valid_datetime(make_valid_dt(2026, 13, 10, 1, 0, 0, 0)));
    EXPECT_FALSE(bsw::RtcService::is_valid_datetime(make_valid_dt(2026, 5, 0, 1, 0, 0, 0)));
    EXPECT_FALSE(bsw::RtcService::is_valid_datetime(make_valid_dt(2026, 5, 32, 1, 0, 0, 0)));
    EXPECT_FALSE(bsw::RtcService::is_valid_datetime(make_valid_dt(2026, 5, 10, 0, 0, 0, 0)));
    EXPECT_FALSE(bsw::RtcService::is_valid_datetime(make_valid_dt(2026, 5, 10, 8, 0, 0, 0)));
    EXPECT_FALSE(bsw::RtcService::is_valid_datetime(make_valid_dt(2026, 5, 10, 7, 24, 0, 0)));
    EXPECT_FALSE(bsw::RtcService::is_valid_datetime(make_valid_dt(2026, 5, 10, 7, 0, 60, 0)));
    EXPECT_FALSE(bsw::RtcService::is_valid_datetime(make_valid_dt(2026, 5, 10, 7, 0, 0, 60)));
}

TEST_F(RtcServiceTest, EpochMathRoundTripsAcrossBoundaries) {
    struct TestCase {
        hal::RtcDateTime dt;
        uint64_t expected_epoch_ms;
    };

    const std::array<TestCase, 4> cases{{
        {make_valid_dt(2000, 1, 1, 6, 0, 0, 0), 946684800000ULL},
        {make_valid_dt(2024, 2, 29, 4, 12, 34, 56), 1709210096000ULL},
        {make_valid_dt(2026, 11, 19, 4, 18, 39, 10), 1795113550000ULL},
        {make_valid_dt(2099, 12, 31, 4, 23, 59, 59), 4102444799000ULL},
    }};

    for (const auto& tc : cases) {
        const uint64_t epoch_ms = bsw::RtcService::datetime_to_epoch_ms(tc.dt);
        EXPECT_EQ(epoch_ms, tc.expected_epoch_ms);
        const hal::RtcDateTime round_trip = bsw::RtcService::epoch_ms_to_datetime(epoch_ms);
        EXPECT_EQ(round_trip.year, tc.dt.year);
        EXPECT_EQ(round_trip.month, tc.dt.month);
        EXPECT_EQ(round_trip.day, tc.dt.day);
        EXPECT_EQ(round_trip.day_of_week, tc.dt.day_of_week);
        EXPECT_EQ(round_trip.hour, tc.dt.hour);
        EXPECT_EQ(round_trip.minute, tc.dt.minute);
        EXPECT_EQ(round_trip.second, tc.dt.second);
    }

    // Leap rollover: 2024-02-28 23:59:59 + 1s -> 2024-02-29 00:00:00
    const uint64_t leap_eve = bsw::RtcService::datetime_to_epoch_ms(make_valid_dt(2024, 2, 28, 3, 23, 59, 59));
    const hal::RtcDateTime leap_day = bsw::RtcService::epoch_ms_to_datetime(leap_eve + 1000ULL);
    EXPECT_EQ(leap_day.year, 2024u);
    EXPECT_EQ(leap_day.month, 2u);
    EXPECT_EQ(leap_day.day, 29u);
    EXPECT_EQ(leap_day.hour, 0u);
    EXPECT_EQ(leap_day.minute, 0u);
    EXPECT_EQ(leap_day.second, 0u);

    // Non-leap rollover: 2025-02-28 23:59:59 + 1s -> 2025-03-01 00:00:00
    const uint64_t non_leap_eve = bsw::RtcService::datetime_to_epoch_ms(make_valid_dt(2025, 2, 28, 5, 23, 59, 59));
    const hal::RtcDateTime march_first = bsw::RtcService::epoch_ms_to_datetime(non_leap_eve + 1000ULL);
    EXPECT_EQ(march_first.year, 2025u);
    EXPECT_EQ(march_first.month, 3u);
    EXPECT_EQ(march_first.day, 1u);
    EXPECT_EQ(march_first.hour, 0u);
    EXPECT_EQ(march_first.minute, 0u);
    EXPECT_EQ(march_first.second, 0u);

    // Invalid datetime returns 0
    EXPECT_EQ(bsw::RtcService::datetime_to_epoch_ms(make_valid_dt(1999, 1, 1, 5, 0, 0, 0)), 0ULL);
}

TEST_F(RtcServiceTest, FormatsSessionTimestampWhenSynchronized) {
    const hal::RtcDateTime dt = make_valid_dt(2026, 11, 19, 4, 18, 39, 10);
    const hal::RtcDeviceStatus ok_status{false};

    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_rtc_hal_, read_datetime(_, _))
        .WillOnce(DoAll(SetArgReferee<0>(dt), SetArgReferee<1>(ok_status), Return(true)));
    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms())
        .WillOnce(Return(1000ULL))
        .WillOnce(Return(1000ULL))
        .WillOnce(Return(1000ULL));

    ASSERT_TRUE(service_.initialize());

    std::array<char, 32> buf{};
    const std::size_t len = service_.format_session_timestamp(buf.data(), buf.size());
    EXPECT_EQ(len, 15u);
    EXPECT_STREQ(buf.data(), "20261119_183910");
}

TEST_F(RtcServiceTest, FormatsBootFallbackSessionTimestampWhenDegraded) {
    EXPECT_CALL(mock_rtc_hal_, init(_)).WillOnce(Return(false));
    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms()).WillOnce(Return(12345ULL));

    EXPECT_FALSE(service_.initialize());

    std::array<char, 32> buf{};
    const std::size_t len = service_.format_session_timestamp(buf.data(), buf.size());
    EXPECT_EQ(len, 17u);
    EXPECT_STREQ(buf.data(), "BOOT_0000012345ms");
}

TEST_F(RtcServiceTest, FormatSessionTimestampHandlesNullAndSmallBuffersSafely) {
    EXPECT_EQ(service_.format_session_timestamp(nullptr, 32u), 0u);

    std::array<char, 8> tiny{};
    tiny[0] = 'X';
    EXPECT_EQ(service_.format_session_timestamp(tiny.data(), 0u), 0u);
    EXPECT_EQ(tiny[0], 'X');

    EXPECT_CALL(mock_rtc_hal_, get_monotonic_ms()).WillOnce(Return(42ULL));
    EXPECT_EQ(service_.format_session_timestamp(tiny.data(), tiny.size()), 0u);
    EXPECT_EQ(tiny[0], '\0');
}

TEST(Ds3231I2cLldTest, CodecBcdAndTimekeepingRoundTrip) {
    const hal::RtcDateTime dt = make_valid_dt(2026, 11, 19, 4, 18, 39, 10);
    std::array<uint8_t, lld::Ds3231I2cLld::kTimekeepingRegCount> regs{};

    ASSERT_TRUE(lld::Ds3231I2cLld::encode_timekeeping_registers(dt, regs));
    EXPECT_EQ(regs[0], 0x10u); // second = 10
    EXPECT_EQ(regs[1], 0x39u); // minute = 39
    EXPECT_EQ(regs[2], 0x18u); // hour = 18 (24h mode, bit 6 = 0)
    EXPECT_EQ(regs[3], 0x04u); // day_of_week = 4
    EXPECT_EQ(regs[4], 0x19u); // date = 19
    EXPECT_EQ(regs[5], 0x11u); // month = 11, century = 0
    EXPECT_EQ(regs[6], 0x26u); // year = 26

    hal::RtcDateTime decoded{};
    ASSERT_TRUE(lld::Ds3231I2cLld::decode_timekeeping_registers(regs, decoded));
    EXPECT_EQ(decoded.year, 2026u);
    EXPECT_EQ(decoded.month, 11u);
    EXPECT_EQ(decoded.day, 19u);
    EXPECT_EQ(decoded.day_of_week, 4u);
    EXPECT_EQ(decoded.hour, 18u);
    EXPECT_EQ(decoded.minute, 39u);
    EXPECT_EQ(decoded.second, 10u);
}

TEST(Ds3231I2cLldTest, CodecSupports12HourAmPmAndRejectsInvalidHours) {
    std::array<uint8_t, lld::Ds3231I2cLld::kTimekeepingRegCount> regs{
        0x00u, 0x15u, 0x52u, 0x01u, 0x01u, 0x01u, 0x26u // 12 AM (bit6=1, bit5=0, 0x12)
    };
    hal::RtcDateTime decoded{};
    ASSERT_TRUE(lld::Ds3231I2cLld::decode_timekeeping_registers(regs, decoded));
    EXPECT_EQ(decoded.hour, 0u);

    regs[2] = 0x72u; // 12 PM (bit6=1, bit5=1, 0x12)
    ASSERT_TRUE(lld::Ds3231I2cLld::decode_timekeeping_registers(regs, decoded));
    EXPECT_EQ(decoded.hour, 12u);

    regs[2] = 0x67u; // 07 PM (bit6=1, bit5=1, 0x07 -> 19)
    ASSERT_TRUE(lld::Ds3231I2cLld::decode_timekeeping_registers(regs, decoded));
    EXPECT_EQ(decoded.hour, 19u);

    regs[2] = 0x40u; // 00 in 12h mode (invalid)
    EXPECT_FALSE(lld::Ds3231I2cLld::decode_timekeeping_registers(regs, decoded));

    regs[2] = 0x53u; // 13 in 12h mode (invalid)
    EXPECT_FALSE(lld::Ds3231I2cLld::decode_timekeeping_registers(regs, decoded));

    regs[2] = 0x24u; // 24 in 24h mode (invalid)
    EXPECT_FALSE(lld::Ds3231I2cLld::decode_timekeeping_registers(regs, decoded));
}

TEST(Ds3231I2cLldTest, CodecRejectsInvalidBcdNibblesAndOutOfRangeFields) {
    uint8_t val = 0u;
    EXPECT_FALSE(lld::Ds3231I2cLld::bcd_to_dec(0x0Au, val));
    EXPECT_FALSE(lld::Ds3231I2cLld::bcd_to_dec(0xA0u, val));
    EXPECT_FALSE(lld::Ds3231I2cLld::bcd_to_dec(0xFFu, val));
    EXPECT_FALSE(lld::Ds3231I2cLld::dec_to_bcd(100u, val));

    std::array<uint8_t, lld::Ds3231I2cLld::kTimekeepingRegCount> regs{
        0xFFu, 0x00u, 0x00u, 0x01u, 0x01u, 0x01u, 0x26u
    };
    hal::RtcDateTime decoded{};
    EXPECT_FALSE(lld::Ds3231I2cLld::decode_timekeeping_registers(regs, decoded));

    std::array<uint8_t, lld::Ds3231I2cLld::kTimekeepingRegCount> out_regs{};
    EXPECT_FALSE(lld::Ds3231I2cLld::encode_timekeeping_registers(make_valid_dt(1999, 12, 31, 5, 0, 0, 0), out_regs));
    EXPECT_FALSE(lld::Ds3231I2cLld::encode_timekeeping_registers(make_valid_dt(2100, 1, 1, 5, 0, 0, 0), out_regs));
}

TEST(Ds3231I2cLldTest, CodecDecodesCenturyBitTo2100sAndServiceRejectsIt) {
    const std::array<uint8_t, lld::Ds3231I2cLld::kTimekeepingRegCount> regs{
        0x00u, 0x00u, 0x00u, 0x05u, 0x01u, 0x81u, 0x00u // Century=1, month=1, yy=00 -> 2100-01-01
    };
    hal::RtcDateTime decoded{};
    ASSERT_TRUE(lld::Ds3231I2cLld::decode_timekeeping_registers(regs, decoded));
    EXPECT_EQ(decoded.year, 2100u);
    EXPECT_FALSE(bsw::RtcService::is_valid_datetime(decoded));
}

TEST(Ds3231I2cLldTest, CodecOsfAndEoscBitHelpersPreserveOtherBits) {
    EXPECT_TRUE(lld::Ds3231I2cLld::is_oscillator_stopped(0x88u));
    EXPECT_FALSE(lld::Ds3231I2cLld::is_oscillator_stopped(0x08u));
    EXPECT_EQ(lld::Ds3231I2cLld::clear_oscillator_stopped_bit(0x8Fu), 0x0Fu);
    EXPECT_EQ(lld::Ds3231I2cLld::enable_oscillator_control_bit(0x9Cu), 0x1Cu);
}

TEST(Ds3231I2cLldTest, InitValidatesConfigAndOperatesWithHostStub) {
    lld::Ds3231I2cLld lld;

    hal::RtcI2cConfig bad_nc{};
    bad_nc.sda_pin = GPIO_NUM_NC;
    EXPECT_FALSE(lld.init(bad_nc));

    hal::RtcI2cConfig bad_flash{};
    bad_flash.scl_pin = GPIO_NUM_26;
    EXPECT_FALSE(lld.init(bad_flash));
    bad_flash.scl_pin = GPIO_NUM_32;
    EXPECT_FALSE(lld.init(bad_flash));

    hal::RtcI2cConfig bad_same{};
    bad_same.scl_pin = PIN_I2C_SDA;
    EXPECT_FALSE(lld.init(bad_same));

    hal::RtcI2cConfig bad_clk{};
    bad_clk.clk_speed_hz = 0u;
    EXPECT_FALSE(lld.init(bad_clk));

    hal::RtcI2cConfig bad_addr{};
    bad_addr.device_address = 0x80u;
    EXPECT_FALSE(lld.init(bad_addr));

    hal::RtcI2cConfig bad_timeout{};
    bad_timeout.timeout_ms = 0;
    EXPECT_FALSE(lld.init(bad_timeout));

    hal::RtcI2cConfig bad_port{};
    bad_port.i2c_port = static_cast<uint8_t>(I2C_NUM_MAX);
    EXPECT_FALSE(lld.init(bad_port));

    // Read/write before init fail cleanly
    hal::RtcDateTime dt{};
    hal::RtcDeviceStatus status{};
    EXPECT_FALSE(lld.read_datetime(dt, status));
    EXPECT_FALSE(lld.write_datetime(make_valid_dt()));

    // Valid default config succeeds against Host stub
    ASSERT_TRUE(lld.init(hal::RtcI2cConfig{}));
    EXPECT_TRUE(lld.is_initialized());
    EXPECT_TRUE(lld.write_datetime(make_valid_dt()));
    // Re-init safety
    ASSERT_TRUE(lld.init(hal::RtcI2cConfig{}));
    EXPECT_TRUE(lld.is_initialized());
}

} // namespace datalogger::sil
