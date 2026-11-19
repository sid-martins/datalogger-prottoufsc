#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <limits>
#include "adc_service.hpp"
#include "mock_adc_hal.hpp"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgReferee;

namespace datalogger::sil {

class AdcServiceTest : public ::testing::Test {
protected:
    MockAdcHal mock_adc_hal_;
    bsw::AdcService service_{mock_adc_hal_};
};

TEST_F(AdcServiceTest, InitializeSucceedsAndFailsAccordingToHal) {
    EXPECT_CALL(mock_adc_hal_, init()).WillOnce(Return(false));
    EXPECT_FALSE(service_.initialize());
    EXPECT_FALSE(service_.is_initialized());

    EXPECT_CALL(mock_adc_hal_, init()).WillOnce(Return(true));
    EXPECT_TRUE(service_.initialize());
    EXPECT_TRUE(service_.is_initialized());
}

TEST_F(AdcServiceTest, SampleAllReadsAllTenAdc1Channels) {
    EXPECT_CALL(mock_adc_hal_, init()).WillOnce(Return(true));
    ASSERT_TRUE(service_.initialize());

    for (std::size_t i = 0; i < hal::kAdcChannelCount; ++i) {
        const auto ch = static_cast<hal::AdcChannelId>(i);
        const int mv = static_cast<int>((i + 1u) * 200u);
        EXPECT_CALL(mock_adc_hal_, read_millivolts(ch, _))
            .WillOnce(DoAll(SetArgReferee<1>(mv), Return(true)));
    }

    ASSERT_TRUE(service_.sample_all(1000u));
    EXPECT_EQ(service_.sample_count(), 10u);
    EXPECT_EQ(service_.read_error_count(), 0u);
    EXPECT_EQ(service_.snapshot().last_update_ms, 1000u);

    for (std::size_t i = 0; i < hal::kAdcChannelCount; ++i) {
        const auto ch = static_cast<hal::AdcChannelId>(i);
        const auto& reading = service_.channel_reading(ch);
        const int expected_mv = static_cast<int>((i + 1u) * 200u);
        EXPECT_TRUE(reading.valid);
        EXPECT_EQ(reading.raw_mv, expected_mv);
        EXPECT_FLOAT_EQ(reading.filtered_mv, static_cast<float>(expected_mv));
        EXPECT_NEAR(reading.physical_value, static_cast<float>(expected_mv) * 0.001f, 1e-5f);
        EXPECT_EQ(reading.timestamp_ms, 1000u);
    }
}

TEST_F(AdcServiceTest, EmaFilterConvergesImmediatelyOnFirstSampleAndSmoothsStepInput) {
    EXPECT_CALL(mock_adc_hal_, init()).WillOnce(Return(true));
    ASSERT_TRUE(service_.initialize());
    ASSERT_TRUE(service_.set_channel_filter_alpha(hal::AdcChannelId::kOilPressure, 0.25f));

    EXPECT_CALL(mock_adc_hal_, read_millivolts(hal::AdcChannelId::kOilPressure, _))
        .WillOnce(DoAll(SetArgReferee<1>(1000), Return(true)))
        .WillOnce(DoAll(SetArgReferee<1>(2000), Return(true)))
        .WillOnce(DoAll(SetArgReferee<1>(2000), Return(true)));

    ASSERT_TRUE(service_.sample_channel(hal::AdcChannelId::kOilPressure, 10u));
    EXPECT_FLOAT_EQ(service_.channel_reading(hal::AdcChannelId::kOilPressure).filtered_mv, 1000.0f);

    ASSERT_TRUE(service_.sample_channel(hal::AdcChannelId::kOilPressure, 20u));
    EXPECT_FLOAT_EQ(service_.channel_reading(hal::AdcChannelId::kOilPressure).filtered_mv, 1250.0f);

    ASSERT_TRUE(service_.sample_channel(hal::AdcChannelId::kOilPressure, 30u));
    EXPECT_FLOAT_EQ(service_.channel_reading(hal::AdcChannelId::kOilPressure).filtered_mv, 1437.5f);
}

TEST_F(AdcServiceTest, AppliesLinearCalibrationPerChannel) {
    EXPECT_CALL(mock_adc_hal_, init()).WillOnce(Return(true));
    ASSERT_TRUE(service_.initialize());

    // OilPressure: 0..3300 mV -> 0..10 bar
    ASSERT_TRUE(service_.set_channel_calibration(
        hal::AdcChannelId::kOilPressure, 10.0f / 3300.0f, 0.0f));
    // OilTemp: 0..3300 mV -> -40..150 C
    ASSERT_TRUE(service_.set_channel_calibration(
        hal::AdcChannelId::kOilTemp, 190.0f / 3300.0f, -40.0f));
    // Lambda: 0..3300 mV -> 0.65..1.35
    ASSERT_TRUE(service_.set_channel_calibration(
        hal::AdcChannelId::kLambda, 0.70f / 3300.0f, 0.65f));

    EXPECT_CALL(mock_adc_hal_, read_millivolts(hal::AdcChannelId::kOilPressure, _))
        .WillOnce(DoAll(SetArgReferee<1>(1650), Return(true)));
    EXPECT_CALL(mock_adc_hal_, read_millivolts(hal::AdcChannelId::kOilTemp, _))
        .WillOnce(DoAll(SetArgReferee<1>(3300), Return(true)));
    EXPECT_CALL(mock_adc_hal_, read_millivolts(hal::AdcChannelId::kLambda, _))
        .WillOnce(DoAll(SetArgReferee<1>(1650), Return(true)));

    ASSERT_TRUE(service_.sample_channel(hal::AdcChannelId::kOilPressure, 100u));
    ASSERT_TRUE(service_.sample_channel(hal::AdcChannelId::kOilTemp, 100u));
    ASSERT_TRUE(service_.sample_channel(hal::AdcChannelId::kLambda, 100u));

    EXPECT_NEAR(service_.channel_reading(hal::AdcChannelId::kOilPressure).physical_value, 5.0f, 1e-4f);
    EXPECT_NEAR(service_.channel_reading(hal::AdcChannelId::kOilTemp).physical_value, 150.0f, 1e-4f);
    EXPECT_NEAR(service_.channel_reading(hal::AdcChannelId::kLambda).physical_value, 1.00f, 1e-4f);
}

TEST_F(AdcServiceTest, SampleTickRespectsPerChannelDividersForMultiRateNyquist) {
    EXPECT_CALL(mock_adc_hal_, init()).WillOnce(Return(true));
    ASSERT_TRUE(service_.initialize());

    // OilPressure divisor = 1 (amostrado em todo tick, ex: 100 Hz)
    ASSERT_TRUE(service_.set_channel_sample_divider(hal::AdcChannelId::kOilPressure, 1u));
    // OilTemp divisor = 10 (amostrado a cada 10 ticks, ex: 10 Hz)
    ASSERT_TRUE(service_.set_channel_sample_divider(hal::AdcChannelId::kOilTemp, 10u));
    // Demais canais com divisor = 5 (amostrados nos ticks 0 e 5)
    for (std::size_t i = 2; i < hal::kAdcChannelCount; ++i) {
        ASSERT_TRUE(service_.set_channel_sample_divider(static_cast<hal::AdcChannelId>(i), 5u));
    }

    // Em 10 ticks (0..9):
    // - kOilPressure (div 1): 10 leituras
    // - kOilTemp (div 10): 1 leitura (tick 0)
    // - Outros 8 canais (div 5): 2 leituras cada (ticks 0 e 5) -> 16 leituras
    EXPECT_CALL(mock_adc_hal_, read_millivolts(hal::AdcChannelId::kOilPressure, _))
        .Times(10)
        .WillRepeatedly(DoAll(SetArgReferee<1>(1500), Return(true)));
    EXPECT_CALL(mock_adc_hal_, read_millivolts(hal::AdcChannelId::kOilTemp, _))
        .Times(1)
        .WillRepeatedly(DoAll(SetArgReferee<1>(1200), Return(true)));

    for (std::size_t i = 2; i < hal::kAdcChannelCount; ++i) {
        EXPECT_CALL(mock_adc_hal_, read_millivolts(static_cast<hal::AdcChannelId>(i), _))
            .Times(2)
            .WillRepeatedly(DoAll(SetArgReferee<1>(1000), Return(true)));
    }

    for (uint64_t t = 0; t < 10u; ++t) {
        EXPECT_TRUE(service_.sample_tick(t * 10u));
    }

    EXPECT_EQ(service_.tick_count(), 10u);
    EXPECT_EQ(service_.sample_count(), 27u);
}

TEST_F(AdcServiceTest, RejectsHalFailureAndOutOfRangeMillivolts) {
    EXPECT_CALL(mock_adc_hal_, init()).WillOnce(Return(true));
    ASSERT_TRUE(service_.initialize());

    EXPECT_CALL(mock_adc_hal_, read_millivolts(hal::AdcChannelId::kFuelPressure, _))
        .WillOnce(Return(false))
        .WillOnce(DoAll(SetArgReferee<1>(-1), Return(true)))
        .WillOnce(DoAll(SetArgReferee<1>(3301), Return(true)));

    EXPECT_FALSE(service_.sample_channel(hal::AdcChannelId::kFuelPressure, 100u));
    EXPECT_FALSE(service_.channel_reading(hal::AdcChannelId::kFuelPressure).valid);

    EXPECT_FALSE(service_.sample_channel(hal::AdcChannelId::kFuelPressure, 200u));
    EXPECT_FALSE(service_.channel_reading(hal::AdcChannelId::kFuelPressure).valid);

    EXPECT_FALSE(service_.sample_channel(hal::AdcChannelId::kFuelPressure, 300u));
    EXPECT_FALSE(service_.channel_reading(hal::AdcChannelId::kFuelPressure).valid);

    EXPECT_EQ(service_.read_error_count(), 3u);
    EXPECT_EQ(service_.sample_count(), 0u);
}

TEST_F(AdcServiceTest, RejectsInvalidFilterAlphaValues) {
    EXPECT_FALSE(service_.set_channel_filter_alpha(hal::AdcChannelId::kLambda, 0.0f));
    EXPECT_FALSE(service_.set_channel_filter_alpha(hal::AdcChannelId::kLambda, -0.1f));
    EXPECT_FALSE(service_.set_channel_filter_alpha(hal::AdcChannelId::kLambda, 1.01f));
    EXPECT_FALSE(service_.set_channel_filter_alpha(
        hal::AdcChannelId::kLambda, std::numeric_limits<float>::quiet_NaN()));

    EXPECT_TRUE(service_.set_channel_filter_alpha(hal::AdcChannelId::kLambda, 1.0f));
    EXPECT_TRUE(service_.set_channel_filter_alpha(hal::AdcChannelId::kLambda, 0.15f));
}

} // namespace datalogger::sil
