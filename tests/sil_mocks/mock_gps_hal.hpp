#pragma once

#include <gmock/gmock.h>
#include "i_gps_hal.hpp"

namespace datalogger::sil {

class MockGpsHal : public hal::IGpsHal {
public:
    MOCK_METHOD(bool, init, (const hal::GpsConfig& config), (override));
    MOCK_METHOD(int, read_bytes, (uint8_t* buffer, std::size_t max_len, uint32_t timeout_ms), (override));
    MOCK_METHOD(bool, write_bytes, (const uint8_t* data, std::size_t len), (override));
};

} // namespace datalogger::sil
