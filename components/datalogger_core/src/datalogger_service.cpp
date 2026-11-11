#include "datalogger_service.hpp"

#include <cmath>
#include <cstdio>

namespace datalogger {

DataloggerService::DataloggerService(hal::ISensor& sensor,
                                     hal::IStorage& storage,
                                     hal::IRtosQueue<hal::SensorReading>& sample_queue,
                                     std::string_view log_file_path,
                                     uint32_t sync_every_n_samples)
    : sensor_(sensor),
      storage_(storage),
      sample_queue_(sample_queue),
      log_file_path_(log_file_path),
      sync_every_n_samples_(sync_every_n_samples == 0 ? 1 : sync_every_n_samples) {}

bool DataloggerService::initialize() {
    if (!sensor_.init()) {
        return false;
    }
    if (!storage_.mount()) {
        return false;
    }
    return true;
}

bool DataloggerService::acquisition_step(uint32_t queue_timeout_ms) {
    hal::SensorReading reading{};
    if (!sensor_.read(reading)) {
        ++read_errors_;
        return false;
    }

    if (!is_reading_valid(reading)) {
        ++read_errors_;
        return false;
    }

    if (!sample_queue_.send(reading, queue_timeout_ms)) {
        ++dropped_samples_;
        return false;
    }

    return true;
}

bool DataloggerService::storage_step(uint32_t queue_timeout_ms) {
    hal::SensorReading reading{};
    if (!sample_queue_.receive(reading, queue_timeout_ms)) {
        return false;
    }

    const std::string csv_line = format_csv_line(reading);
    if (!storage_.append_line(log_file_path_, csv_line)) {
        return false;
    }

    ++samples_logged_;
    if (samples_logged_ % sync_every_n_samples_ == 0) {
        if (!storage_.sync()) {
            return false;
        }
    }

    return true;
}

std::string DataloggerService::format_csv_line(const hal::SensorReading& reading) {
    char buffer[128];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "%llu,%.2f,%.2f,%.1f",
                  static_cast<unsigned long long>(reading.timestamp_ms),
                  static_cast<double>(reading.temperature_c),
                  static_cast<double>(reading.humidity_pct),
                  static_cast<double>(reading.battery_mv));
    return std::string(buffer);
}

bool DataloggerService::is_reading_valid(const hal::SensorReading& reading) {
    if (std::isnan(reading.temperature_c) || std::isnan(reading.humidity_pct) ||
        std::isnan(reading.battery_mv)) {
        return false;
    }
    if (reading.temperature_c < kMinValidTempC || reading.temperature_c > kMaxValidTempC) {
        return false;
    }
    if (reading.humidity_pct < 0.0f || reading.humidity_pct > 100.0f) {
        return false;
    }
    if (reading.battery_mv < 0.0f) {
        return false;
    }
    return true;
}

} // namespace datalogger

