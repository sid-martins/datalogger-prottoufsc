#pragma once

#include <gmock/gmock.h>
#include "i_can_driver.hpp"

namespace datalogger::testing {

class MockCanDriver : public hal::ICanDriver {
public:
    MOCK_METHOD(bool, init, (const hal::CanConfig& config), (override));
    MOCK_METHOD(bool, start, (), (override));
    MOCK_METHOD(bool, stop, (), (override));
    MOCK_METHOD(bool, transmit, (const hal::CanFrame& frame, uint32_t timeout_ms), (override));
    MOCK_METHOD(bool, receive, (hal::CanFrame& out_frame, uint32_t timeout_ms), (override));
    MOCK_METHOD(hal::CanBusState, get_state, (), (const, override));
    MOCK_METHOD(bool, initiate_recovery, (), (override));
};

} // namespace datalogger::testing

