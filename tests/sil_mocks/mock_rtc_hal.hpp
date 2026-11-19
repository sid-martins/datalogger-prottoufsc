#pragma once

#include <gmock/gmock.h>
#include "i_rtc_hal.hpp"

namespace datalogger::sil {

class MockRtcHal : public hal::IRtcHal {
public:
    MOCK_METHOD(bool, init, (const hal::RtcI2cConfig& config), (override));
    MOCK_METHOD(bool, read_datetime, (hal::RtcDateTime& out_dt, hal::RtcDeviceStatus& out_status), (override));
    MOCK_METHOD(bool, write_datetime, (const hal::RtcDateTime& dt), (override));
    MOCK_METHOD(uint64_t, get_monotonic_ms, (), (const, override));
};

} // namespace datalogger::sil
