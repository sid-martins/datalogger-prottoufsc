#pragma once

#include <gmock/gmock.h>
#include "i_sensor.hpp"

namespace datalogger::testing {

class MockSensor : public hal::ISensor {
public:
    MOCK_METHOD(bool, init, (), (override));
    MOCK_METHOD(bool, read, (hal::SensorReading& out_reading), (override));
};

} // namespace datalogger::testing

