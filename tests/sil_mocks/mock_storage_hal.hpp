#pragma once

#include <gmock/gmock.h>
#include "i_storage_hal.hpp"

namespace datalogger::sil {

class MockStorageHal : public hal::IStorageHal {
public:
    MOCK_METHOD(bool, mount, (const hal::StorageConfig& config), (override));
    MOCK_METHOD(bool, unmount, (), (override));
    MOCK_METHOD(bool, is_mounted, (), (const, override));
    MOCK_METHOD(bool, open_file, (std::string_view file_path), (override));
    MOCK_METHOD(bool, write, (const char* data, std::size_t len), (override));
    MOCK_METHOD(bool, sync, (), (override));
    MOCK_METHOD(bool, close_file, (), (override));
    MOCK_METHOD(bool, get_free_space_bytes, (uint64_t& out_free_bytes), (const, override));
};

} // namespace datalogger::sil
