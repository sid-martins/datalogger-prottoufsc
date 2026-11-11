#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "bsp_pins.h"
#include "can_service.hpp"
#include "fake_osal_queue.hpp"
#include "mock_can_hal.hpp"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Field;
using ::testing::Return;
using ::testing::SetArgReferee;

namespace datalogger::sil {

class CanServiceTest : public ::testing::Test {
protected:
    MockCanHal mock_can_hal_;
    FakeOsalQueue<hal::CanFrame> rx_queue_{4};
    bsw::CanService service_{mock_can_hal_};
};

TEST_F(CanServiceTest, DefaultConfigUsesBspPinsAnd500Kbps) {
    const hal::CanConfig default_cfg{};
    EXPECT_EQ(default_cfg.tx_pin, PIN_CAN_TX);
    EXPECT_EQ(default_cfg.tx_pin, GPIO_NUM_15);
    EXPECT_EQ(default_cfg.rx_pin, PIN_CAN_RX);
    EXPECT_EQ(default_cfg.rx_pin, GPIO_NUM_16);
    EXPECT_EQ(default_cfg.baud_rate, hal::CanBaudRate::k500Kbps);
}

TEST_F(CanServiceTest, InitializeInstallsHalAndStartsBus) {
    EXPECT_CALL(mock_can_hal_,
                init(AllOf(Field(&hal::CanConfig::tx_pin, GPIO_NUM_15),
                           Field(&hal::CanConfig::rx_pin, GPIO_NUM_16),
                           Field(&hal::CanConfig::baud_rate, hal::CanBaudRate::k500Kbps))))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_can_hal_, start()).WillOnce(Return(true));

    EXPECT_TRUE(service_.initialize());
    EXPECT_TRUE(service_.is_initialized());
}

TEST_F(CanServiceTest, InitializeFailsWhenHalInitFails) {
    EXPECT_CALL(mock_can_hal_, init(_)).WillOnce(Return(false));
    EXPECT_CALL(mock_can_hal_, start()).Times(0);

    EXPECT_FALSE(service_.initialize());
    EXPECT_FALSE(service_.is_initialized());
}

TEST_F(CanServiceTest, SendFrameTransmitsValidStandard11BitFrame) {
    hal::CanFrame frame{};
    frame.id = 0x7E0;
    frame.is_extended = false;
    frame.dlc = 8;
    frame.data = {0x02, 0x01, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00};

    EXPECT_CALL(mock_can_hal_, transmit(_, 50)).WillOnce(Return(true));

    EXPECT_TRUE(service_.send_frame(frame, 50));
    EXPECT_EQ(service_.tx_count(), 1u);
    EXPECT_EQ(service_.tx_error_count(), 0u);
}

TEST_F(CanServiceTest, SendFrameTransmitsValidExtended29BitFrame) {
    hal::CanFrame frame{};
    frame.id = 0x18DAF110;
    frame.is_extended = true;
    frame.dlc = 4;
    frame.data = {0x10, 0x20, 0x30, 0x40};

    EXPECT_CALL(mock_can_hal_, transmit(_, 50)).WillOnce(Return(true));

    EXPECT_TRUE(service_.send_frame(frame, 50));
    EXPECT_EQ(service_.tx_count(), 1u);
    EXPECT_EQ(service_.tx_error_count(), 0u);
}

TEST_F(CanServiceTest, SendFrameRejectsStandardIdExceeding11Bits) {
    hal::CanFrame invalid_std_frame{};
    invalid_std_frame.id = 0x800;
    invalid_std_frame.is_extended = false;
    invalid_std_frame.dlc = 2;

    EXPECT_CALL(mock_can_hal_, transmit(_, _)).Times(0);

    EXPECT_FALSE(service_.send_frame(invalid_std_frame, 50));
    EXPECT_EQ(service_.tx_count(), 0u);
    EXPECT_EQ(service_.tx_error_count(), 1u);
}

TEST_F(CanServiceTest, SendFrameRejectsExtendedIdExceeding29Bits) {
    hal::CanFrame invalid_ext_frame{};
    invalid_ext_frame.id = 0x20000000;
    invalid_ext_frame.is_extended = true;
    invalid_ext_frame.dlc = 2;

    EXPECT_CALL(mock_can_hal_, transmit(_, _)).Times(0);

    EXPECT_FALSE(service_.send_frame(invalid_ext_frame, 50));
    EXPECT_EQ(service_.tx_count(), 0u);
    EXPECT_EQ(service_.tx_error_count(), 1u);
}

TEST_F(CanServiceTest, SendFrameRejectsDlcGreaterThan8) {
    hal::CanFrame invalid_dlc_frame{};
    invalid_dlc_frame.id = 0x123;
    invalid_dlc_frame.is_extended = false;
    invalid_dlc_frame.dlc = 9;

    EXPECT_CALL(mock_can_hal_, transmit(_, _)).Times(0);

    EXPECT_FALSE(service_.send_frame(invalid_dlc_frame, 50));
    EXPECT_EQ(service_.tx_error_count(), 1u);
}

TEST_F(CanServiceTest, ReceiveToQueueEnqueuesIncomingFrameViaOsal) {
    hal::CanFrame incoming{};
    incoming.id = 0x360;
    incoming.is_extended = false;
    incoming.dlc = 8;
    incoming.data = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    incoming.timestamp_ms = 2500;

    EXPECT_CALL(mock_can_hal_, receive(_, 100))
        .WillOnce(DoAll(SetArgReferee<0>(incoming), Return(true)));

    EXPECT_TRUE(service_.receive_to_queue(rx_queue_, 100, 10));
    EXPECT_EQ(service_.rx_count(), 1u);
    EXPECT_EQ(rx_queue_.messages_waiting(), 1u);

    hal::CanFrame dequeued{};
    ASSERT_TRUE(rx_queue_.receive(dequeued, 0));
    EXPECT_EQ(dequeued.id, 0x360u);
    EXPECT_EQ(dequeued.timestamp_ms, 2500u);
    EXPECT_EQ(dequeued.data[0], 0x12);
}

TEST_F(CanServiceTest, ReceiveToQueueCountsDroppedFramesWhenOsalQueueIsFull) {
    FakeOsalQueue<hal::CanFrame> full_queue(1);
    hal::CanFrame incoming{};
    incoming.id = 0x100;
    incoming.dlc = 1;

    EXPECT_CALL(mock_can_hal_, receive(_, 50))
        .Times(2)
        .WillRepeatedly(DoAll(SetArgReferee<0>(incoming), Return(true)));

    EXPECT_TRUE(service_.receive_to_queue(full_queue, 50, 10));
    EXPECT_FALSE(service_.receive_to_queue(full_queue, 50, 10));
    EXPECT_EQ(service_.dropped_rx_count(), 1u);
}

TEST_F(CanServiceTest, CheckAndRecoverInitiatesRecoveryWhenBusOff) {
    EXPECT_CALL(mock_can_hal_, get_state()).WillOnce(Return(hal::CanBusState::kBusOff));
    EXPECT_CALL(mock_can_hal_, initiate_recovery()).WillOnce(Return(true));

    EXPECT_TRUE(service_.check_and_recover_bus());
    EXPECT_EQ(service_.bus_off_recovery_count(), 1u);
}

} // namespace datalogger::sil

