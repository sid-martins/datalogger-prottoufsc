#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "board_pins.h"
#include "can_controller.hpp"
#include "mock_can_driver.hpp"
#include "mock_rtos_queue.hpp"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Field;
using ::testing::Return;
using ::testing::SetArgReferee;

namespace datalogger::testing {

class CanControllerTest : public ::testing::Test {
protected:
    MockCanDriver mock_driver_;
    FakeRtosQueue<hal::CanFrame> rx_queue_{4};
    can::CanController controller_{mock_driver_};
};

TEST_F(CanControllerTest, DefaultConfigUsesSchematicPinsAnd500Kbps) {
    const hal::CanConfig default_cfg{};
    EXPECT_EQ(default_cfg.tx_pin, PIN_CAN_TX);
    EXPECT_EQ(default_cfg.tx_pin, GPIO_NUM_15);
    EXPECT_EQ(default_cfg.rx_pin, PIN_CAN_RX);
    EXPECT_EQ(default_cfg.rx_pin, GPIO_NUM_16);
    EXPECT_EQ(default_cfg.baud_rate, hal::CanBaudRate::k500Kbps);
}

TEST_F(CanControllerTest, InitializeInstallsDriverAndStartsBus) {
    EXPECT_CALL(mock_driver_,
                init(AllOf(Field(&hal::CanConfig::tx_pin, GPIO_NUM_15),
                           Field(&hal::CanConfig::rx_pin, GPIO_NUM_16),
                           Field(&hal::CanConfig::baud_rate, hal::CanBaudRate::k500Kbps))))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_driver_, start()).WillOnce(Return(true));

    EXPECT_TRUE(controller_.initialize());
    EXPECT_TRUE(controller_.is_initialized());
}

TEST_F(CanControllerTest, InitializeFailsWhenDriverInitFails) {
    EXPECT_CALL(mock_driver_, init(_)).WillOnce(Return(false));
    EXPECT_CALL(mock_driver_, start()).Times(0);

    EXPECT_FALSE(controller_.initialize());
    EXPECT_FALSE(controller_.is_initialized());
}

TEST_F(CanControllerTest, SendFrameTransmitsValidStandard11BitFrame) {
    hal::CanFrame frame{};
    frame.id = 0x7E0; // OBD2 / ECU Standard ID (11-bit <= 0x7FF)
    frame.is_extended = false;
    frame.dlc = 8;
    frame.data = {0x02, 0x01, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00};

    EXPECT_CALL(mock_driver_, transmit(_, 50)).WillOnce(Return(true));

    EXPECT_TRUE(controller_.send_frame(frame, 50));
    EXPECT_EQ(controller_.tx_count(), 1u);
    EXPECT_EQ(controller_.tx_error_count(), 0u);
}

TEST_F(CanControllerTest, SendFrameTransmitsValidExtended29BitFrame) {
    hal::CanFrame frame{};
    frame.id = 0x18DAF110; // Extended 29-bit CAN ID (<= 0x1FFFFFFF)
    frame.is_extended = true;
    frame.dlc = 4;
    frame.data = {0x10, 0x20, 0x30, 0x40};

    EXPECT_CALL(mock_driver_, transmit(_, 50)).WillOnce(Return(true));

    EXPECT_TRUE(controller_.send_frame(frame, 50));
    EXPECT_EQ(controller_.tx_count(), 1u);
    EXPECT_EQ(controller_.tx_error_count(), 0u);
}

TEST_F(CanControllerTest, SendFrameRejectsStandardIdExceeding11Bits) {
    hal::CanFrame invalid_std_frame{};
    invalid_std_frame.id = 0x800; // Inválido para Standard 11-bit (máx 0x7FF)
    invalid_std_frame.is_extended = false;
    invalid_std_frame.dlc = 2;

    EXPECT_CALL(mock_driver_, transmit(_, _)).Times(0);

    EXPECT_FALSE(controller_.send_frame(invalid_std_frame, 50));
    EXPECT_EQ(controller_.tx_count(), 0u);
    EXPECT_EQ(controller_.tx_error_count(), 1u);
}

TEST_F(CanControllerTest, SendFrameRejectsExtendedIdExceeding29Bits) {
    hal::CanFrame invalid_ext_frame{};
    invalid_ext_frame.id = 0x20000000; // Inválido para Extended 29-bit (máx 0x1FFFFFFF)
    invalid_ext_frame.is_extended = true;
    invalid_ext_frame.dlc = 2;

    EXPECT_CALL(mock_driver_, transmit(_, _)).Times(0);

    EXPECT_FALSE(controller_.send_frame(invalid_ext_frame, 50));
    EXPECT_EQ(controller_.tx_count(), 0u);
    EXPECT_EQ(controller_.tx_error_count(), 1u);
}

TEST_F(CanControllerTest, SendFrameRejectsDlcGreaterThan8) {
    hal::CanFrame invalid_dlc_frame{};
    invalid_dlc_frame.id = 0x123;
    invalid_dlc_frame.is_extended = false;
    invalid_dlc_frame.dlc = 9; // CAN 2.0 permite no máximo 8 bytes

    EXPECT_CALL(mock_driver_, transmit(_, _)).Times(0);

    EXPECT_FALSE(controller_.send_frame(invalid_dlc_frame, 50));
    EXPECT_EQ(controller_.tx_error_count(), 1u);
}

TEST_F(CanControllerTest, ReceiveToQueueEnqueuesIncomingFrameFromTransceiver) {
    hal::CanFrame incoming{};
    incoming.id = 0x360;
    incoming.is_extended = false;
    incoming.dlc = 8;
    incoming.data = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    incoming.timestamp_ms = 2500;

    EXPECT_CALL(mock_driver_, receive(_, 100))
        .WillOnce(DoAll(SetArgReferee<0>(incoming), Return(true)));

    EXPECT_TRUE(controller_.receive_to_queue(rx_queue_, 100, 10));
    EXPECT_EQ(controller_.rx_count(), 1u);
    EXPECT_EQ(rx_queue_.messages_waiting(), 1u);

    hal::CanFrame dequeued{};
    ASSERT_TRUE(rx_queue_.receive(dequeued, 0));
    EXPECT_EQ(dequeued.id, 0x360u);
    EXPECT_EQ(dequeued.timestamp_ms, 2500u);
    EXPECT_EQ(dequeued.data[0], 0x12);
}

TEST_F(CanControllerTest, ReceiveToQueueCountsDroppedFramesWhenQueueIsFull) {
    FakeRtosQueue<hal::CanFrame> full_queue(1);
    hal::CanFrame incoming{};
    incoming.id = 0x100;
    incoming.dlc = 1;

    EXPECT_CALL(mock_driver_, receive(_, 50))
        .Times(2)
        .WillRepeatedly(DoAll(SetArgReferee<0>(incoming), Return(true)));

    EXPECT_TRUE(controller_.receive_to_queue(full_queue, 50, 10));  // Ocupa a única vaga
    EXPECT_FALSE(controller_.receive_to_queue(full_queue, 50, 10)); // Fila cheia -> drop
    EXPECT_EQ(controller_.dropped_rx_count(), 1u);
}

TEST_F(CanControllerTest, CheckAndRecoverInitiatesRecoveryWhenBusOff) {
    EXPECT_CALL(mock_driver_, get_state()).WillOnce(Return(hal::CanBusState::kBusOff));
    EXPECT_CALL(mock_driver_, initiate_recovery()).WillOnce(Return(true));

    EXPECT_TRUE(controller_.check_and_recover_bus());
    EXPECT_EQ(controller_.bus_off_recovery_count(), 1u);
}

} // namespace datalogger::testing

