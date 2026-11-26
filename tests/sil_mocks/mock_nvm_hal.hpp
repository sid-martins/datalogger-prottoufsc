#pragma once

#include <gmock/gmock.h>
#include "i_nvm_hal.hpp"

namespace datalogger::sil {

/**
 * @brief Mock GMock para `hal::INvmHal` nos testes SIL do Horímetro de Motor.
 */
class MockNvmHal : public hal::INvmHal {
public:
    MOCK_METHOD(bool, init, (const hal::NvmConfig& config), (override));
    MOCK_METHOD(hal::NvmReadResult, read_u32, (const char* key, uint32_t& out_value), (const, override));
    MOCK_METHOD(bool, write_u32, (const char* key, uint32_t value), (override));
    MOCK_METHOD(bool, commit, (), (override));
};

} // namespace datalogger::sil
