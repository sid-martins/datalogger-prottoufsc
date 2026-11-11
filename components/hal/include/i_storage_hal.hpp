#pragma once

#include <cstddef>
#include <string_view>

namespace datalogger::hal {

/**
 * @brief HAL (Hardware Abstraction Layer) para armazenamento não-volátil (MicroSD J4 / Flash).
 */
class IStorageHal {
public:
    virtual ~IStorageHal() = default;

    virtual bool mount() = 0;
    virtual bool append_line(std::string_view file_path, std::string_view line) = 0;
    virtual bool sync() = 0;
};

} // namespace datalogger::hal

