#include "gps_service.hpp"

#include <cmath>
#include <limits>

namespace datalogger::bsw {

namespace {

bool parse_hex_nibble(char c, uint8_t& out_nibble) {
    if (c >= '0' && c <= '9') {
        out_nibble = static_cast<uint8_t>(c - '0');
        return true;
    }
    if (c >= 'A' && c <= 'F') {
        out_nibble = static_cast<uint8_t>(10 + (c - 'A'));
        return true;
    }
    if (c >= 'a' && c <= 'f') {
        out_nibble = static_cast<uint8_t>(10 + (c - 'a'));
        return true;
    }
    return false;
}

void strip_trailing_crlf(std::string_view& sv) {
    while (!sv.empty() && (sv.back() == '\r' || sv.back() == '\n')) {
        sv.remove_suffix(1u);
    }
}

bool parse_uint(std::string_view s, uint32_t& out_val) {
    if (s.empty()) {
        return false;
    }
    uint64_t accum = 0u;
    const uint64_t max_u32 = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max());
    for (char c : s) {
        if (c < '0' || c > '9') {
            return false;
        }
        accum = (accum * 10u) + static_cast<uint64_t>(c - '0');
        if (accum > max_u32) {
            return false;
        }
    }
    out_val = static_cast<uint32_t>(accum);
    return true;
}

bool parse_double(std::string_view s, double& out_val) {
    if (s.empty()) {
        return false;
    }

    std::size_t idx = 0u;
    bool negative = false;
    if (s[idx] == '-') {
        negative = true;
        ++idx;
    } else if (s[idx] == '+') {
        ++idx;
    }

    if (idx >= s.size()) {
        return false;
    }

    bool seen_dot = false;
    std::size_t digit_count = 0u;
    double int_part = 0.0;
    double frac_part = 0.0;
    double frac_div = 1.0;

    for (; idx < s.size(); ++idx) {
        const char c = s[idx];
        if (c == '.') {
            if (seen_dot) {
                return false;
            }
            seen_dot = true;
            continue;
        }
        if (c < '0' || c > '9') {
            return false;
        }
        ++digit_count;
        const double digit = static_cast<double>(c - '0');
        if (!seen_dot) {
            int_part = (int_part * 10.0) + digit;
            if (!std::isfinite(int_part)) {
                return false;
            }
        } else if (frac_div < 1e18) {
            frac_part = (frac_part * 10.0) + digit;
            frac_div *= 10.0;
        }
    }

    if (digit_count == 0u) {
        return false;
    }

    const double magnitude = int_part + (frac_part / frac_div);
    const double val = negative ? -magnitude : magnitude;
    if (!std::isfinite(val)) {
        return false;
    }
    out_val = val;
    return true;
}

bool to_finite_float(double val, float& out_val) {
    const double max_float = static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(val) || !(val >= -max_float && val <= max_float)) {
        return false;
    }
    const float converted = static_cast<float>(val);
    if (!std::isfinite(converted)) {
        return false;
    }
    out_val = converted;
    return true;
}

bool parse_nmea_coord(std::string_view coord_str,
                      std::string_view hemi_str,
                      bool is_longitude,
                      double& out_deg) {
    if (coord_str.empty() || hemi_str.size() != 1u) {
        return false;
    }

    const std::size_t deg_digits = is_longitude ? 3u : 2u;
    const std::size_t dot_pos = coord_str.find('.');
    const std::size_t int_len = (dot_pos == std::string_view::npos) ? coord_str.size() : dot_pos;
    if (int_len != (deg_digits + 2u)) {
        return false;
    }

    uint32_t deg = 0u;
    if (!parse_uint(coord_str.substr(0u, deg_digits), deg)) {
        return false;
    }

    const std::string_view min_str = coord_str.substr(deg_digits);
    if (min_str.front() == '+' || min_str.front() == '-') {
        return false;
    }
    double minutes = 0.0;
    if (!parse_double(min_str, minutes) || !(minutes >= 0.0 && minutes < 60.0)) {
        return false;
    }

    double decimal = static_cast<double>(deg) + (minutes / 60.0);
    const char hemi = hemi_str.front();
    if (is_longitude) {
        if (!(decimal >= 0.0 && decimal <= 180.0)) {
            return false;
        }
        if (hemi == 'W') {
            decimal = -decimal;
        } else if (hemi != 'E') {
            return false;
        }
    } else {
        if (!(decimal >= 0.0 && decimal <= 90.0)) {
            return false;
        }
        if (hemi == 'S') {
            decimal = -decimal;
        } else if (hemi != 'N') {
            return false;
        }
    }

    out_deg = decimal;
    return true;
}

bool parse_utc_time_ms(std::string_view s, uint32_t& out_ms) {
    if (s.size() < 6u) {
        return false;
    }
    uint32_t hh = 0u;
    uint32_t mm = 0u;
    uint32_t ss = 0u;
    if (!parse_uint(s.substr(0u, 2u), hh) ||
        !parse_uint(s.substr(2u, 2u), mm) ||
        !parse_uint(s.substr(4u, 2u), ss)) {
        return false;
    }
    if (hh > 23u || mm > 59u || ss > 59u) {
        return false;
    }

    uint32_t ms = 0u;
    if (s.size() > 6u) {
        if (s[6u] != '.') {
            return false;
        }
        const std::string_view frac = s.substr(7u);
        if (frac.empty()) {
            return false;
        }
        uint32_t weights[3] = {100u, 10u, 1u};
        for (std::size_t i = 0u; i < frac.size(); ++i) {
            if (frac[i] < '0' || frac[i] > '9') {
                return false;
            }
            if (i < 3u) {
                ms += static_cast<uint32_t>(frac[i] - '0') * weights[i];
            }
        }
    }

    out_ms = (((hh * 3600u) + (mm * 60u) + ss) * 1000u) + ms;
    return true;
}

bool parse_utc_date_yyyymmdd(std::string_view s, uint32_t& out_yyyymmdd) {
    if (s.size() != 6u) {
        return false;
    }
    uint32_t day = 0u;
    uint32_t month = 0u;
    uint32_t yy = 0u;
    if (!parse_uint(s.substr(0u, 2u), day) ||
        !parse_uint(s.substr(2u, 2u), month) ||
        !parse_uint(s.substr(4u, 2u), yy)) {
        return false;
    }

    const uint32_t year = 2000u + yy;
    if (month < 1u || month > 12u) {
        return false;
    }
    const uint8_t max_day = RtcService::days_in_month(static_cast<uint16_t>(year),
                                                      static_cast<uint8_t>(month));
    if (day < 1u || day > static_cast<uint32_t>(max_day)) {
        return false;
    }

    out_yyyymmdd = (year * 10000u) + (month * 100u) + day;
    return true;
}

} // namespace

GpsService::GpsService(hal::IGpsHal& gps_hal, const RtcService& rtc_service)
    : gps_hal_(gps_hal), rtc_service_(rtc_service) {}

void GpsService::reset_parser_state() {
    line_len_ = 0u;
    line_overflowed_ = false;
    frame_state_ = FrameState::kIdle;
    ubx_len_lsb_ = 0u;
    ubx_remaining_ = 0u;
}

bool GpsService::initialize(const hal::GpsConfig& config) {
    initialized_ = false;
    latest_data_ = hal::GpsData{};
    reset_parser_state();
    sentences_parsed_ = 0u;
    checksum_errors_ = 0u;
    rx_bytes_ = 0u;
    overflow_count_ = 0u;

    if (!gps_hal_.init(config)) {
        return false;
    }
    initialized_ = true;
    return true;
}

bool GpsService::poll(uint32_t timeout_ms) {
    if (!initialized_) {
        return false;
    }

    std::array<uint8_t, kRxChunkSize> rx_buf{};
    const int read_len = gps_hal_.read_bytes(rx_buf.data(), rx_buf.size(), timeout_ms);
    if (read_len <= 0) {
        return false;
    }

    const std::size_t raw_count = static_cast<std::size_t>(read_len);
    const std::size_t count = (raw_count <= rx_buf.size()) ? raw_count : rx_buf.size();
    rx_bytes_ += static_cast<uint32_t>(count);

    bool parsed_valid = false;
    for (std::size_t i = 0u; i < count; ++i) {
        if (process_byte(rx_buf[i])) {
            parsed_valid = true;
        }
    }
    return parsed_valid;
}

bool GpsService::process_byte(uint8_t byte) {
    if (frame_state_ == FrameState::kUbxSync2) {
        if (byte == 0x62u) {
            frame_state_ = FrameState::kUbxClass;
            return false;
        }
        if (byte == 0xB5u) {
            return false;
        }
        frame_state_ = FrameState::kIdle;
    } else if (frame_state_ == FrameState::kUbxClass) {
        frame_state_ = FrameState::kUbxId;
        return false;
    } else if (frame_state_ == FrameState::kUbxId) {
        frame_state_ = FrameState::kUbxLenL;
        return false;
    } else if (frame_state_ == FrameState::kUbxLenL) {
        ubx_len_lsb_ = byte;
        frame_state_ = FrameState::kUbxLenH;
        return false;
    } else if (frame_state_ == FrameState::kUbxLenH) {
        const std::size_t payload_len =
            static_cast<std::size_t>(ubx_len_lsb_) | (static_cast<std::size_t>(byte) << 8u);
        if (payload_len > kMaxUbxSkipLen) {
            frame_state_ = FrameState::kIdle;
            return false;
        }
        ubx_remaining_ = payload_len + 2u; // +2 para CK_A e CK_B
        frame_state_ = FrameState::kUbxSkip;
        return false;
    } else if (frame_state_ == FrameState::kUbxSkip) {
        if (ubx_remaining_ > 0u) {
            --ubx_remaining_;
        }
        if (ubx_remaining_ == 0u) {
            frame_state_ = FrameState::kIdle;
        }
        return false;
    }

    if (byte == 0xB5u) {
        line_len_ = 0u;
        line_overflowed_ = false;
        frame_state_ = FrameState::kUbxSync2;
        return false;
    }

    if (byte == static_cast<uint8_t>('$')) {
        line_len_ = 0u;
        line_overflowed_ = false;
        line_buf_[line_len_++] = '$';
        frame_state_ = FrameState::kNmeaBody;
        return false;
    }

    if (byte == static_cast<uint8_t>('\r') || byte == static_cast<uint8_t>('\n')) {
        bool parsed = false;
        if (frame_state_ == FrameState::kNmeaBody && line_len_ > 0u && !line_overflowed_) {
            parsed = parse_nmea_sentence(std::string_view(line_buf_.data(), line_len_));
        }
        line_len_ = 0u;
        line_overflowed_ = false;
        frame_state_ = FrameState::kIdle;
        return parsed;
    }

    if (frame_state_ == FrameState::kNmeaBody) {
        if (byte < 0x20u || byte > 0x7Eu) {
            line_len_ = 0u;
            line_overflowed_ = false;
            frame_state_ = FrameState::kIdle;
            return false;
        }
        if (line_len_ < kLineBufferSize) {
            line_buf_[line_len_++] = static_cast<char>(byte);
        } else {
            ++overflow_count_;
            line_overflowed_ = true;
            line_len_ = 0u;
            frame_state_ = FrameState::kIdle;
        }
    }

    return false;
}

bool GpsService::validate_nmea_checksum(std::string_view sentence) {
    strip_trailing_crlf(sentence);
    if (sentence.size() < 4u || sentence.front() != '$') {
        return false;
    }

    const std::size_t star_pos = sentence.find('*');
    if (star_pos == std::string_view::npos ||
        star_pos < 1u ||
        sentence.size() != (star_pos + 3u)) {
        return false;
    }

    const std::string_view payload = sentence.substr(1u, star_pos - 1u);
    if (payload.find('$') != std::string_view::npos) {
        return false;
    }

    uint8_t hi = 0u;
    uint8_t lo = 0u;
    if (!parse_hex_nibble(sentence[star_pos + 1u], hi) ||
        !parse_hex_nibble(sentence[star_pos + 2u], lo)) {
        return false;
    }
    const uint8_t expected = static_cast<uint8_t>((hi << 4u) | lo);

    uint8_t computed = 0u;
    for (char c : payload) {
        computed ^= static_cast<uint8_t>(c);
    }

    return computed == expected;
}

bool GpsService::parse_nmea_sentence(std::string_view sentence) {
    strip_trailing_crlf(sentence);
    if (!validate_nmea_checksum(sentence)) {
        ++checksum_errors_;
        return false;
    }

    const std::size_t star_pos = sentence.find('*');
    const std::string_view payload = sentence.substr(1u, star_pos - 1u);

    std::array<std::string_view, kMaxNmeaFields> fields{};
    std::size_t field_count = 0u;
    std::size_t token_start = 0u;

    for (std::size_t i = 0u; i <= payload.size(); ++i) {
        if (i == payload.size() || payload[i] == ',') {
            if (field_count >= kMaxNmeaFields) {
                return false;
            }
            fields[field_count++] = payload.substr(token_start, i - token_start);
            token_start = i + 1u;
        }
    }

    if (field_count < 1u || fields[0].size() != 5u) {
        return false;
    }

    const std::string_view type = fields[0].substr(2u, 3u);
    if (type == "GGA") {
        if (!parse_gga(fields, field_count)) {
            return false;
        }
        stamp_rtc_time();
        ++sentences_parsed_;
        return true;
    }

    if (type == "RMC") {
        if (!parse_rmc(fields, field_count)) {
            return false;
        }
        stamp_rtc_time();
        ++sentences_parsed_;
        return true;
    }

    // Outras sentenças válidas (TXT, GSA, GSV, GLL, VTG) são ignoradas sem erro
    return false;
}

bool GpsService::parse_gga(const std::array<std::string_view, kMaxNmeaFields>& fields,
                           std::size_t field_count) {
    if (field_count != 15u) {
        return false;
    }

    hal::GpsData next = latest_data_;

    if (!fields[1].empty()) {
        uint32_t utc_ms = 0u;
        if (!parse_utc_time_ms(fields[1], utc_ms)) {
            return false;
        }
        next.utc_time_ms = utc_ms;
    }

    uint32_t fix_quality = 0u;
    if (!fields[6].empty()) {
        if (!parse_uint(fields[6], fix_quality) || fix_quality > 8u) {
            return false;
        }
    }
    next.fix_quality = static_cast<uint8_t>(fix_quality);

    uint32_t sats = 0u;
    if (!fields[7].empty()) {
        if (!parse_uint(fields[7], sats) || sats > 99u) {
            return false;
        }
    }
    next.satellites = static_cast<uint8_t>(sats);

    if (!fields[8].empty()) {
        double hdop = 0.0;
        float hdop_f = 0.0f;
        if (!parse_double(fields[8], hdop) ||
            !(hdop >= 0.0) ||
            !to_finite_float(hdop, hdop_f)) {
            return false;
        }
        next.hdop = hdop_f;
    } else {
        next.hdop = 99.99f;
    }

    if (!fields[9].empty()) {
        double alt = 0.0;
        float alt_f = 0.0f;
        if (!parse_double(fields[9], alt) || !to_finite_float(alt, alt_f)) {
            return false;
        }
        next.altitude_m = alt_f;
    } else {
        next.altitude_m = 0.0f;
    }

    if (fix_quality > 0u) {
        double lat = 0.0;
        double lon = 0.0;
        if (!parse_nmea_coord(fields[2], fields[3], false, lat) ||
            !parse_nmea_coord(fields[4], fields[5], true, lon)) {
            return false;
        }
        next.latitude_deg = lat;
        next.longitude_deg = lon;
        next.gga_valid = true;
    } else {
        next.gga_valid = false;
    }

    next.has_fix = next.gga_valid || next.rmc_valid;
    if (!next.has_fix) {
        next.latitude_deg = 0.0;
        next.longitude_deg = 0.0;
    }

    latest_data_ = next;
    return true;
}

bool GpsService::parse_rmc(const std::array<std::string_view, kMaxNmeaFields>& fields,
                           std::size_t field_count) {
    if (field_count < 12u || field_count > 14u) {
        return false;
    }

    hal::GpsData next = latest_data_;

    if (!fields[1].empty()) {
        uint32_t utc_ms = 0u;
        if (!parse_utc_time_ms(fields[1], utc_ms)) {
            return false;
        }
        next.utc_time_ms = utc_ms;
    }

    if (fields[2].size() != 1u || (fields[2].front() != 'A' && fields[2].front() != 'V')) {
        return false;
    }

    if (!fields[9].empty()) {
        uint32_t yyyymmdd = 0u;
        if (!parse_utc_date_yyyymmdd(fields[9], yyyymmdd)) {
            return false;
        }
        next.utc_date_yyyymmdd = yyyymmdd;
    }

    bool pos_mode_valid = true;
    if (field_count >= 13u && !fields[12].empty()) {
        if (fields[12].size() != 1u) {
            return false;
        }
        if (fields[12].front() == 'N') {
            pos_mode_valid = false;
        }
    }

    if (fields[2].front() == 'A' && pos_mode_valid) {
        double lat = 0.0;
        double lon = 0.0;
        if (!parse_nmea_coord(fields[3], fields[4], false, lat) ||
            !parse_nmea_coord(fields[5], fields[6], true, lon)) {
            return false;
        }
        next.latitude_deg = lat;
        next.longitude_deg = lon;

        if (!fields[7].empty()) {
            double knots = 0.0;
            float knots_f = 0.0f;
            if (!parse_double(fields[7], knots) ||
                !(knots >= 0.0) ||
                !to_finite_float(knots, knots_f)) {
                return false;
            }
            const float kmh = knots_f * kKnotsToKmh;
            const float mps = knots_f * kKnotsToMps;
            if (!std::isfinite(kmh) || !std::isfinite(mps)) {
                return false;
            }
            next.speed_knots = knots_f;
            next.speed_kmh = kmh;
            next.speed_mps = mps;
        } else {
            next.speed_knots = 0.0f;
            next.speed_kmh = 0.0f;
            next.speed_mps = 0.0f;
        }

        if (!fields[8].empty()) {
            double course = 0.0;
            float course_f = 0.0f;
            if (!parse_double(fields[8], course) ||
                !(course >= 0.0 && course <= 360.0) ||
                !to_finite_float(course, course_f)) {
                return false;
            }
            next.course_deg = course_f;
        } else {
            next.course_deg = 0.0f;
        }

        next.rmc_valid = true;
    } else {
        next.rmc_valid = false;
        next.speed_knots = 0.0f;
        next.speed_kmh = 0.0f;
        next.speed_mps = 0.0f;
        next.course_deg = 0.0f;
    }

    next.has_fix = next.gga_valid || next.rmc_valid;
    if (!next.has_fix) {
        next.latitude_deg = 0.0;
        next.longitude_deg = 0.0;
    }

    latest_data_ = next;
    return true;
}

void GpsService::stamp_rtc_time() {
    const RtcTimestamp ts = rtc_service_.sample_timestamp();
    latest_data_.timestamp_ms = ts.epoch_ms;
    latest_data_.monotonic_ms = ts.monotonic_ms;
    latest_data_.time_synchronized = ts.synchronized;
}

} // namespace datalogger::bsw
