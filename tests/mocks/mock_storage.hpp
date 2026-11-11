#pragma once

#include <gmock/gmock.h>
#include "i_storage.hpp"

namespace datalogger::testing {

class MockStorage : public hal::IStorage {
public:
    MOCK_METHOD(bool, mount, (), (override));
    MOCK_METHOD(bool, append_line, (std::string_view file_path, std::string_view line), (override));
    MOCK_METHOD(bool, sync, (), (override));
};

} // namespace datalogger::testing

