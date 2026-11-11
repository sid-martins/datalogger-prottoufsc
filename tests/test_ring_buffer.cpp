#include <gtest/gtest.h>
#include "i_can_hal.hpp"
#include "ring_buffer.hpp"

namespace datalogger::sil {

TEST(RingBufferTest, StartsEmptyWithExpectedCapacity) {
    bsw::RingBuffer<hal::CanFrame, 4> buffer;
    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());
    EXPECT_EQ(buffer.size(), 0u);
    EXPECT_EQ(buffer.capacity(), 4u);
}

TEST(RingBufferTest, PushAndPopCanFramesFollowFifoOrder) {
    bsw::RingBuffer<hal::CanFrame, 3> buffer;

    hal::CanFrame frame1{};
    frame1.id = 0x100;
    frame1.dlc = 2;
    frame1.data = {0xAA, 0xBB};

    hal::CanFrame frame2{};
    frame2.id = 0x200;
    frame2.dlc = 1;
    frame2.data = {0xCC};

    EXPECT_TRUE(buffer.push(frame1));
    EXPECT_TRUE(buffer.push(frame2));
    EXPECT_EQ(buffer.size(), 2u);

    hal::CanFrame out{};
    ASSERT_TRUE(buffer.pop(out));
    EXPECT_EQ(out.id, 0x100u);
    EXPECT_EQ(out.data[0], 0xAA);

    ASSERT_TRUE(buffer.pop(out));
    EXPECT_EQ(out.id, 0x200u);
    EXPECT_EQ(out.data[0], 0xCC);

    EXPECT_TRUE(buffer.empty());
}

TEST(RingBufferTest, RejectsPushWhenFullAndOverwriteDisabled) {
    bsw::RingBuffer<int, 2> buffer;
    EXPECT_TRUE(buffer.push(10));
    EXPECT_TRUE(buffer.push(20));
    EXPECT_TRUE(buffer.full());

    EXPECT_FALSE(buffer.push(30, false));
    EXPECT_EQ(buffer.size(), 2u);
}

TEST(RingBufferTest, OverwritesOldestElementWhenOverwriteEnabled) {
    bsw::RingBuffer<int, 2> buffer;
    EXPECT_TRUE(buffer.push(10));
    EXPECT_TRUE(buffer.push(20));

    EXPECT_TRUE(buffer.push(30, true));
    EXPECT_EQ(buffer.size(), 2u);

    int first = 0;
    int second = 0;
    ASSERT_TRUE(buffer.pop(first));
    ASSERT_TRUE(buffer.pop(second));
    EXPECT_EQ(first, 20);
    EXPECT_EQ(second, 30);
}

} // namespace datalogger::sil
