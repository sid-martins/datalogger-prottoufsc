#include <gtest/gtest.h>
#include "ring_buffer.hpp"

namespace datalogger::testing {

TEST(SampleRingBufferTest, StartsEmptyWithExpectedCapacity) {
    SampleRingBuffer<4> buffer;
    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());
    EXPECT_EQ(buffer.size(), 0u);
    EXPECT_EQ(buffer.capacity(), 4u);
}

TEST(SampleRingBufferTest, PushAndPopFollowFifoOrder) {
    SampleRingBuffer<3> buffer;

    EXPECT_TRUE(buffer.push({1000, 22.5f, 50.0f, 3800.0f}));
    EXPECT_TRUE(buffer.push({2000, 23.0f, 51.0f, 3790.0f}));
    EXPECT_EQ(buffer.size(), 2u);

    hal::SensorReading out{};
    ASSERT_TRUE(buffer.pop(out));
    EXPECT_EQ(out.timestamp_ms, 1000u);
    EXPECT_FLOAT_EQ(out.temperature_c, 22.5f);

    ASSERT_TRUE(buffer.pop(out));
    EXPECT_EQ(out.timestamp_ms, 2000u);
    EXPECT_FLOAT_EQ(out.temperature_c, 23.0f);

    EXPECT_TRUE(buffer.empty());
}

TEST(SampleRingBufferTest, RejectsPushWhenFullAndOverwriteDisabled) {
    SampleRingBuffer<2> buffer;
    EXPECT_TRUE(buffer.push({100, 20.0f, 40.0f, 3700.0f}));
    EXPECT_TRUE(buffer.push({200, 21.0f, 41.0f, 3700.0f}));
    EXPECT_TRUE(buffer.full());

    // Não deve sobrescrever por padrão
    EXPECT_FALSE(buffer.push({300, 22.0f, 42.0f, 3700.0f}, false));
    EXPECT_EQ(buffer.size(), 2u);
}

TEST(SampleRingBufferTest, OverwritesOldestSampleWhenOverwriteEnabled) {
    SampleRingBuffer<2> buffer;
    EXPECT_TRUE(buffer.push({100, 20.0f, 40.0f, 3700.0f}));
    EXPECT_TRUE(buffer.push({200, 21.0f, 41.0f, 3700.0f}));

    // Com overwrite = true, a amostra de t=100 é descartada e t=300 entra
    EXPECT_TRUE(buffer.push({300, 22.0f, 42.0f, 3700.0f}, true));
    EXPECT_EQ(buffer.size(), 2u);

    hal::SensorReading first{};
    ASSERT_TRUE(buffer.pop(first));
    EXPECT_EQ(first.timestamp_ms, 200u);

    hal::SensorReading second{};
    ASSERT_TRUE(buffer.pop(second));
    EXPECT_EQ(second.timestamp_ms, 300u);
}

} // namespace datalogger::testing

