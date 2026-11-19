#pragma once

#include <gmock/gmock.h>
#include "i_adc_hal.hpp"

namespace datalogger::sil {

class MockAdcHal : public hal::IAdcHal {
public:
    MOCK_METHOD(bool, init, (), (override));
    MOCK_METHOD(bool, read_raw, (hal::AdcChannelId channel, int& out_raw), (override));
    MOCK_METHOD(bool, read_millivolts, (hal::AdcChannelId channel, int& out_mv), (override));
};

} // namespace datalogger::sil
