#pragma once

#include <gmock/gmock.h>
#include "i_storage_hal.hpp"

namespace datalogger::sil {

class MockStorageHal : public hal::IStorageHal {
public:
    MOCK_METHOD(bool, mount, (), (override));
    MOCK_METHOD(bool, append_line, (std::string_view file_path, std::string_view line), (override));
    MOCK_METHOD(bool, sync, (), (override));
};

} // namespace datalogger::sil

