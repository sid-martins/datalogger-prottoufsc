#pragma once

#include <gmock/gmock.h>
#include "i_gpio_hal.hpp"

namespace datalogger::sil {

class MockGpioHal : public hal::IGpioHal {
public:
    MOCK_METHOD(bool, init, (const hal::UiGpioConfig& config), (override));
    MOCK_METHOD(bool, read_pin, (gpio_num_t pin, bool& out_high), (const, override));
    MOCK_METHOD(bool, write_pin, (gpio_num_t pin, bool high), (override));
};

} // namespace datalogger::sil
