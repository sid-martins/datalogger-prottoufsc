#include "rtc_service.hpp"

#include <array>

namespace datalogger::bsw {

namespace {

constexpr uint64_t kMsPerSecond = 1000ULL;
constexpr uint64_t kMsPerMinute = 60ULL * kMsPerSecond;
constexpr uint64_t kMsPerHour   = 60ULL * kMsPerMinute;
constexpr uint64_t kMsPerDay    = 24ULL * kMsPerHour;

int64_t days_from_civil(uint16_t y, uint8_t m, uint8_t d) {
    const int64_t y_adj = static_cast<int64_t>(y) - ((m <= 2u) ? 1 : 0);
    const int64_t era   = (y_adj >= 0 ? y_adj : y_adj - 399) / 400;
    const uint64_t yoe  = static_cast<uint64_t>(y_adj - (era * 400));
    const uint64_t m_adj = (m > 2u) ? (static_cast<uint64_t>(m) - 3u)
                                    : (static_cast<uint64_t>(m) + 9u);
    const uint64_t doy = ((153u * m_adj) + 2u) / 5u + static_cast<uint64_t>(d) - 1u;
    const uint64_t doe = (yoe * 365u) + (yoe / 4u) - (yoe / 100u) + doy;
    return (era * 146097) + static_cast<int64_t>(doe) - 719468;
}

void civil_from_days(int64_t z,
                     uint16_t& out_year,
                     uint8_t& out_month,
                     uint8_t& out_day,
                     uint8_t& out_dow) {
    const int64_t z_shifted = z + 719468;
    const int64_t era       = (z_shifted >= 0 ? z_shifted : z_shifted - 146096) / 146097;
    const uint64_t doe      = static_cast<uint64_t>(z_shifted - (era * 146097));
    const uint64_t yoe      = (doe - (doe / 1460u) + (doe / 36524u) - (doe / 146096u)) / 365u;
    int64_t y               = static_cast<int64_t>(yoe) + (era * 400);
    const uint64_t doy      = doe - ((365u * yoe) + (yoe / 4u) - (yoe / 100u));
    const uint64_t mp       = ((5u * doy) + 2u) / 153u;
    const uint64_t d        = doy - (((153u * mp) + 2u) / 5u) + 1u;
    const uint64_t m        = (mp < 10u) ? (mp + 3u) : (mp - 9u);
    y += (m <= 2u) ? 1 : 0;

    if (y < 0) {
        y = 0;
    } else if (y > 65535) {
        y = 65535;
    }

    out_year  = static_cast<uint16_t>(y);
    out_month = static_cast<uint8_t>(m);
    out_day   = static_cast<uint8_t>(d);

    // 1970-01-01 (z = 0) foi quinta-feira (ISO 8601 = 4; 0..6 mapeado para 1..7)
    const int64_t dow_mod = ((z % 7) + 7 + 3) % 7;
    out_dow = static_cast<uint8_t>(dow_mod + 1);
}

void append_padded_uint(char* dst,
                        std::size_t& pos,
                        uint64_t value,
                        std::size_t min_width) {
    std::array<char, 24> digits{};
    std::size_t count = 0u;
    do {
        digits[count++] = static_cast<char>('0' + static_cast<char>(value % 10ULL));
        value /= 10ULL;
    } while (value > 0ULL && count < digits.size());

    while (count < min_width && count < digits.size()) {
        digits[count++] = '0';
    }

    while (count > 0u) {
        --count;
        dst[pos++] = digits[count];
    }
}

} // namespace

RtcService::RtcService(hal::IRtcHal& rtc_hal)
    : rtc_hal_(rtc_hal) {}

bool RtcService::initialize(const hal::RtcI2cConfig& config) {
    hal_initialized_     = false;
    synchronized_        = false;
    status_              = RtcSyncStatus::kUninitialized;
    anchor_monotonic_ms_ = 0ULL;
    anchor_epoch_ms_     = 0ULL;
    sync_count_          = 0u;
    sync_error_count_    = 0u;

    if (!rtc_hal_.init(config)) {
        status_ = RtcSyncStatus::kHalInitFailed;
        ++sync_error_count_;
        return false;
    }

    hal_initialized_ = true;
    return sync_from_hal();
}

bool RtcService::resync() {
    return sync_from_hal();
}

bool RtcService::sync_from_hal() {
    if (!hal_initialized_) {
        synchronized_        = false;
        status_              = RtcSyncStatus::kHalInitFailed;
        anchor_monotonic_ms_ = 0ULL;
        anchor_epoch_ms_     = 0ULL;
        ++sync_error_count_;
        return false;
    }

    hal::RtcDateTime dt{};
    hal::RtcDeviceStatus dev_status{};
    const uint64_t t_before = rtc_hal_.get_monotonic_ms();
    const bool read_ok      = rtc_hal_.read_datetime(dt, dev_status);
    const uint64_t t_after  = rtc_hal_.get_monotonic_ms();

    if (!read_ok) {
        synchronized_        = false;
        status_              = RtcSyncStatus::kI2cReadFailed;
        anchor_monotonic_ms_ = 0ULL;
        anchor_epoch_ms_     = 0ULL;
        ++sync_error_count_;
        return false;
    }

    if (dev_status.oscillator_stopped) {
        synchronized_        = false;
        status_              = RtcSyncStatus::kOscillatorStopped;
        anchor_monotonic_ms_ = 0ULL;
        anchor_epoch_ms_     = 0ULL;
        ++sync_error_count_;
        return false;
    }

    if (!is_valid_datetime(dt)) {
        synchronized_        = false;
        status_              = RtcSyncStatus::kInvalidCalendar;
        anchor_monotonic_ms_ = 0ULL;
        anchor_epoch_ms_     = 0ULL;
        ++sync_error_count_;
        return false;
    }

    const uint64_t delta_ms = (t_after >= t_before) ? (t_after - t_before) : 0ULL;
    anchor_monotonic_ms_    = t_before + (delta_ms / 2ULL);
    anchor_epoch_ms_        = datetime_to_epoch_ms(dt);
    synchronized_           = true;
    status_                 = RtcSyncStatus::kSynchronized;
    ++sync_count_;
    return true;
}

bool RtcService::set_datetime(const hal::RtcDateTime& dt) {
    if (!hal_initialized_ || !is_valid_datetime(dt)) {
        ++sync_error_count_;
        return false;
    }

    const uint64_t t_before = rtc_hal_.get_monotonic_ms();
    const bool write_ok     = rtc_hal_.write_datetime(dt);
    const uint64_t t_after  = rtc_hal_.get_monotonic_ms();

    if (!write_ok) {
        ++sync_error_count_;
        return false;
    }

    const uint64_t delta_ms = (t_after >= t_before) ? (t_after - t_before) : 0ULL;
    anchor_monotonic_ms_    = t_before + (delta_ms / 2ULL);
    anchor_epoch_ms_        = datetime_to_epoch_ms(dt);
    synchronized_           = true;
    status_                 = RtcSyncStatus::kSynchronized;
    ++sync_count_;
    return true;
}

RtcTimestamp RtcService::sample_timestamp() const {
    const uint64_t mono_ms = rtc_hal_.get_monotonic_ms();
    RtcTimestamp ts{};
    ts.monotonic_ms = mono_ms;
    ts.epoch_ms     = monotonic_to_epoch_ms(mono_ms);
    ts.synchronized = synchronized_;
    return ts;
}

uint64_t RtcService::now_epoch_ms() const {
    return monotonic_to_epoch_ms(rtc_hal_.get_monotonic_ms());
}

uint64_t RtcService::monotonic_to_epoch_ms(uint64_t monotonic_ms) const {
    if (!synchronized_) {
        return monotonic_ms;
    }
    if (monotonic_ms >= anchor_monotonic_ms_) {
        return anchor_epoch_ms_ + (monotonic_ms - anchor_monotonic_ms_);
    }
    const uint64_t delta_ms = anchor_monotonic_ms_ - monotonic_ms;
    return (anchor_epoch_ms_ > delta_ms) ? (anchor_epoch_ms_ - delta_ms) : 0ULL;
}

bool RtcService::now_datetime(hal::RtcDateTime& out_dt) const {
    if (!synchronized_) {
        return false;
    }
    out_dt = epoch_ms_to_datetime(now_epoch_ms());
    return true;
}

std::size_t RtcService::format_session_timestamp(char* out_buf, std::size_t buf_size) const {
    if (out_buf == nullptr || buf_size == 0u) {
        return 0u;
    }

    std::array<char, kMinSessionTimestampBufSize> tmp{};
    std::size_t pos = 0u;

    if (synchronized_) {
        const hal::RtcDateTime dt = epoch_ms_to_datetime(now_epoch_ms());
        append_padded_uint(tmp.data(), pos, dt.year, 4u);
        append_padded_uint(tmp.data(), pos, dt.month, 2u);
        append_padded_uint(tmp.data(), pos, dt.day, 2u);
        tmp[pos++] = '_';
        append_padded_uint(tmp.data(), pos, dt.hour, 2u);
        append_padded_uint(tmp.data(), pos, dt.minute, 2u);
        append_padded_uint(tmp.data(), pos, dt.second, 2u);
    } else {
        const uint64_t boot_ms = rtc_hal_.get_monotonic_ms();
        constexpr std::array<char, 5u> kBootPrefix{'B', 'O', 'O', 'T', '_'};
        for (char c : kBootPrefix) {
            tmp[pos++] = c;
        }
        append_padded_uint(tmp.data(), pos, boot_ms, 10u);
        tmp[pos++] = 'm';
        tmp[pos++] = 's';
    }

    if (pos >= buf_size) {
        out_buf[0] = '\0';
        return 0u;
    }

    for (std::size_t i = 0u; i < pos; ++i) {
        out_buf[i] = tmp[i];
    }
    out_buf[pos] = '\0';
    return pos;
}

bool RtcService::is_leap_year(uint16_t year) {
    return (year % 4u == 0u) && ((year % 100u != 0u) || (year % 400u == 0u));
}

uint8_t RtcService::days_in_month(uint16_t year, uint8_t month) {
    switch (month) {
        case 1u:
        case 3u:
        case 5u:
        case 7u:
        case 8u:
        case 10u:
        case 12u:
            return 31u;
        case 4u:
        case 6u:
        case 9u:
        case 11u:
            return 30u;
        case 2u:
            return is_leap_year(year) ? 29u : 28u;
        default:
            return 0u;
    }
}

bool RtcService::is_valid_datetime(const hal::RtcDateTime& dt) {
    if (dt.year < kMinYear || dt.year > kMaxYear) {
        return false;
    }
    const uint8_t max_day = days_in_month(dt.year, dt.month);
    if (max_day == 0u || dt.day < 1u || dt.day > max_day) {
        return false;
    }
    if (dt.day_of_week < 1u || dt.day_of_week > 7u) {
        return false;
    }
    if (dt.hour > 23u || dt.minute > 59u || dt.second > 59u) {
        return false;
    }
    return true;
}

uint64_t RtcService::datetime_to_epoch_ms(const hal::RtcDateTime& dt) {
    if (!is_valid_datetime(dt)) {
        return 0ULL;
    }

    const int64_t days = days_from_civil(dt.year, dt.month, dt.day);
    if (days < 0) {
        return 0ULL;
    }

    const uint64_t day_ms = static_cast<uint64_t>(days) * kMsPerDay;
    const uint64_t tod_ms = (static_cast<uint64_t>(dt.hour) * kMsPerHour) +
                            (static_cast<uint64_t>(dt.minute) * kMsPerMinute) +
                            (static_cast<uint64_t>(dt.second) * kMsPerSecond);
    return day_ms + tod_ms;
}

hal::RtcDateTime RtcService::epoch_ms_to_datetime(uint64_t epoch_ms) {
    const uint64_t total_days = epoch_ms / kMsPerDay;
    const uint64_t rem_ms     = epoch_ms % kMsPerDay;

    hal::RtcDateTime dt{};
    civil_from_days(static_cast<int64_t>(total_days),
                    dt.year,
                    dt.month,
                    dt.day,
                    dt.day_of_week);
    dt.hour   = static_cast<uint8_t>(rem_ms / kMsPerHour);
    dt.minute = static_cast<uint8_t>((rem_ms % kMsPerHour) / kMsPerMinute);
    dt.second = static_cast<uint8_t>((rem_ms % kMsPerMinute) / kMsPerSecond);
    return dt;
}

} // namespace datalogger::bsw
