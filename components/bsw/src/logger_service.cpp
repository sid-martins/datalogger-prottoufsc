#include "logger_service.hpp"

#include <cstring>
#include <iterator>
#include <limits>
#include "bsp_pins.h"

namespace datalogger::bsw {

namespace {

constexpr std::array<uint32_t, 256> make_crc32_table() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0u; i < 256u; ++i) {
        uint32_t crc = i;
        for (int bit = 0; bit < 8; ++bit) {
            crc = ((crc & 1u) != 0u) ? ((crc >> 1u) ^ binlog::kCrc32Poly) : (crc >> 1u);
        }
        table[i] = crc;
    }
    return table;
}

constexpr std::array<uint32_t, 256> kCrc32Table = make_crc32_table();

void write_u8(uint8_t* dst, std::size_t& offset, uint8_t value) {
    dst[offset++] = value;
}

void write_u16_le(uint8_t* dst, std::size_t& offset, uint16_t value) {
    dst[offset++] = static_cast<uint8_t>(value & 0xFFu);
    dst[offset++] = static_cast<uint8_t>((value >> 8u) & 0xFFu);
}

void write_i16_le(uint8_t* dst, std::size_t& offset, int16_t value) {
    write_u16_le(dst, offset, static_cast<uint16_t>(value));
}

void write_u32_le(uint8_t* dst, std::size_t& offset, uint32_t value) {
    dst[offset++] = static_cast<uint8_t>(value & 0xFFu);
    dst[offset++] = static_cast<uint8_t>((value >> 8u) & 0xFFu);
    dst[offset++] = static_cast<uint8_t>((value >> 16u) & 0xFFu);
    dst[offset++] = static_cast<uint8_t>((value >> 24u) & 0xFFu);
}

void write_u64_le(uint8_t* dst, std::size_t& offset, uint64_t value) {
    for (std::size_t i = 0u; i < 8u; ++i) {
        dst[offset++] = static_cast<uint8_t>((value >> (i * 8u)) & 0xFFULL);
    }
}

void write_f32_le(uint8_t* dst, std::size_t& offset, float value) {
    static_assert(sizeof(float) == 4u && std::numeric_limits<float>::is_iec559,
                  "Formato binario exige float IEEE 754 binary32 (4 bytes)");
    uint32_t raw_bits = 0u;
    std::memcpy(&raw_bits, &value, sizeof(raw_bits));
    write_u32_le(dst, offset, raw_bits);
}

void write_f64_le(uint8_t* dst, std::size_t& offset, double value) {
    static_assert(sizeof(double) == 8u && std::numeric_limits<double>::is_iec559,
                  "Formato binario exige double IEEE 754 binary64 (8 bytes)");
    uint64_t raw_bits = 0ULL;
    std::memcpy(&raw_bits, &value, sizeof(raw_bits));
    write_u64_le(dst, offset, raw_bits);
}

void write_fixed_ascii_slot(uint8_t* dst, std::size_t& offset, std::size_t slot_size, std::string_view text) {
    std::memset(dst + offset, 0, slot_size);
    if (slot_size > 1u && !text.empty()) {
        const std::size_t max_copy = slot_size - 1u;
        const std::size_t copy_len = (text.size() < max_copy) ? text.size() : max_copy;
        std::memcpy(dst + offset, text.data(), copy_len);
    }
    offset += slot_size;
}

} // namespace

LoggerService::LoggerService(hal::IStorageHal& storage_hal,
                             uint32_t sync_every_n_samples,
                             uint16_t nominal_rate_hz)
    : storage_hal_(storage_hal),
      sync_every_n_samples_(sync_every_n_samples == 0u ? 1u : sync_every_n_samples),
      nominal_rate_hz_(nominal_rate_hz == 0u ? kDefaultNominalRateHz : nominal_rate_hz) {
    constexpr std::string_view kDefaultMount{"/sdcard"};
    std::memcpy(mount_point_buf_.data(), kDefaultMount.data(), kDefaultMount.size());
    mount_point_buf_[kDefaultMount.size()] = '\0';
    config_.mount_point = mount_point_buf_.data();
}

bool LoggerService::initialize(const hal::StorageConfig& config) {
    config_ = config;
    const char* src_mount = (config.mount_point != nullptr && config.mount_point[0] != '\0')
                                ? config.mount_point
                                : "/sdcard";
    std::size_t len = std::strlen(src_mount);
    if (len >= mount_point_buf_.size()) {
        len = mount_point_buf_.size() - 1u;
    }
    std::memcpy(mount_point_buf_.data(), src_mount, len);
    mount_point_buf_[len] = '\0';
    while (len > 1u && mount_point_buf_[len - 1u] == '/') {
        mount_point_buf_[--len] = '\0';
    }
    config_.mount_point = mount_point_buf_.data();

    return storage_hal_.mount(config_);
}

bool LoggerService::start_session(const RtcService& rtc_service, uint32_t engine_hours_total_s) {
    if (session_active_) {
        return false;
    }

    if (!storage_hal_.is_mounted()) {
        if (!storage_hal_.mount(config_)) {
            ++storage_error_count_;
            return false;
        }
    }

    uint64_t free_bytes = 0ULL;
    if (!storage_hal_.get_free_space_bytes(free_bytes) || free_bytes < kMinFreeSpaceBytes) {
        ++storage_error_count_;
        return false;
    }

    std::array<char, RtcService::kMinSessionTimestampBufSize> ts_buf{};
    const std::size_t ts_len = rtc_service.format_session_timestamp(ts_buf.data(), ts_buf.size());
    if (ts_len == 0u || !build_session_file_path(std::string_view(ts_buf.data(), ts_len))) {
        current_file_path_[0] = '\0';
        ++storage_error_count_;
        return false;
    }

    if (!storage_hal_.open_file(current_file_path())) {
        ++storage_error_count_;
        return false;
    }

    const RtcTimestamp start_ts = rtc_service.sample_timestamp();
    const binlog::BinarySessionHeaderMeta header_meta{
        start_ts.monotonic_ms,
        start_ts.epoch_ms,
        nominal_rate_hz_,
        start_ts.synchronized,
        engine_hours_total_s
    };

    std::array<uint8_t, kFileHeaderSize> header_buf{};
    const std::size_t header_len = encode_file_header(header_meta, header_buf.data(), header_buf.size());
    if (header_len != kFileHeaderSize ||
        !storage_hal_.write(reinterpret_cast<const char*>(header_buf.data()), header_len) ||
        !storage_hal_.sync()) {
        storage_hal_.close_file();
        ++storage_error_count_;
        return false;
    }

    bytes_written_ += static_cast<uint64_t>(header_len);
    block_len_ = 0u;
    samples_since_sync_ = 0u;
    session_active_ = true;
    ++sync_count_;
    return true;
}

bool LoggerService::log_snapshot(const TelemetrySnapshot& snapshot) {
    if (!session_active_) {
        return false;
    }

    std::array<uint8_t, kRecordSize> record_buf{};
    const std::size_t record_len = encode_telemetry_record(snapshot, record_buf.data(), record_buf.size());
    if (record_len != kRecordSize || record_len > kWriteBlockBufferSize) {
        ++storage_error_count_;
        return false;
    }

    if (block_len_ + record_len > kWriteBlockBufferSize) {
        if (!flush_block()) {
            ++storage_error_count_;
            return false;
        }
    }

    std::memcpy(block_buf_.data() + block_len_, record_buf.data(), record_len);
    block_len_ += record_len;
    ++samples_logged_;
    ++samples_since_sync_;

    if (samples_since_sync_ >= sync_every_n_samples_) {
        if (!flush_block() || !storage_hal_.sync()) {
            ++storage_error_count_;
            return false;
        }
        samples_since_sync_ = 0u;
        ++sync_count_;
    }

    return true;
}

bool LoggerService::process_queue_step(osal::IOsalQueue<TelemetrySnapshot>& queue,
                                         uint32_t receive_timeout_ms) {
    TelemetrySnapshot snap{};
    if (!queue.receive(snap, receive_timeout_ms)) {
        return false;
    }
    return log_snapshot(snap);
}

bool LoggerService::flush() {
    if (!session_active_) {
        return false;
    }
    if (!flush_block() || !storage_hal_.sync()) {
        ++storage_error_count_;
        return false;
    }
    samples_since_sync_ = 0u;
    ++sync_count_;
    return true;
}

bool LoggerService::stop_session() {
    if (!session_active_) {
        return false;
    }

    const bool flushed = flush_block();
    const bool synced = flushed ? storage_hal_.sync() : false;
    const bool closed = storage_hal_.close_file();

    session_active_ = false;
    block_len_ = 0u;
    samples_since_sync_ = 0u;

    if (!flushed || !synced || !closed) {
        ++storage_error_count_;
        return false;
    }

    ++sync_count_;
    return true;
}

uint32_t LoggerService::compute_crc32(const uint8_t* data, std::size_t len) {
    if (data == nullptr || len == 0u) {
        return 0u;
    }

    uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0u; i < len; ++i) {
        crc = (crc >> 8u) ^ kCrc32Table[(crc ^ data[i]) & 0xFFu];
    }
    return crc ^ 0xFFFFFFFFu;
}

std::size_t LoggerService::encode_file_header(const binlog::BinarySessionHeaderMeta& meta,
                                              uint8_t* out_buf,
                                              std::size_t buf_size) {
    if (out_buf == nullptr || buf_size < kFileHeaderSize) {
        return 0u;
    }

    static_assert(hal::kAdcChannelCount == 10u, "Cabeçalho espera 10 canais ADC no BSP");
    static_assert(std::size(bsp::kAnalogChannels) == hal::kAdcChannelCount,
                  "Tabela bsp::kAnalogChannels deve ter 10 entradas");

    std::memset(out_buf, 0, kFileHeaderSize);
    std::size_t offset = 0u;

    // 0..3: Magic "PDLG"
    std::memcpy(out_buf + offset, binlog::kFileMagic.data(), binlog::kFileMagic.size());
    offset += binlog::kFileMagic.size();

    // 4..15: Metadados estruturais do arquivo
    const uint16_t header_flags = meta.rtc_synchronized ? binlog::kHeaderFlagRtcSynced : 0u;
    write_u16_le(out_buf, offset, binlog::kFormatVersion);
    write_u16_le(out_buf, offset, static_cast<uint16_t>(kFileHeaderSize));
    write_u16_le(out_buf, offset, static_cast<uint16_t>(kRecordSize));
    write_u16_le(out_buf, offset, header_flags);
    write_u16_le(out_buf, offset, meta.nominal_rate_hz);
    write_u8(out_buf, offset, static_cast<uint8_t>(hal::kAdcChannelCount));
    write_u8(out_buf, offset, static_cast<uint8_t>(VehicleCanState::kNodeCount));

    // 16..31: Timestamps de abertura da sessão
    write_u64_le(out_buf, offset, meta.start_monotonic_ms);
    write_u64_le(out_buf, offset, meta.start_epoch_ms);

    // 32..191: Nomes dos 10 canais analógicos locais copiados do BSP (TAG_*)
    for (std::size_t i = 0u; i < hal::kAdcChannelCount; ++i) {
        const char* tag = (bsp::kAnalogChannels[i].tag != nullptr) ? bsp::kAnalogChannels[i].tag : "";
        write_fixed_ascii_slot(out_buf, offset, binlog::kAdcTagSlotSize, std::string_view(tag));
    }

    // 192..223: Identificador do dispositivo/versão do hardware
    write_fixed_ascii_slot(out_buf, offset, binlog::kDeviceIdSlotSize, binlog::kDeviceId);

    // 224..227: Horímetro acumulado do motor na abertura da sessão (segundos totais, uint32_t LE)
    write_u32_le(out_buf, offset, meta.engine_hours_total_s);

    // 228..251: Reservado (zerado via memset inicial)
    offset += 24u;

    // 252..255: CRC-32 IEEE 802.3 sobre bytes 0..251
    const uint32_t crc = compute_crc32(out_buf, offset);
    write_u32_le(out_buf, offset, crc);

    return (offset == kFileHeaderSize) ? kFileHeaderSize : 0u;
}

std::size_t LoggerService::encode_telemetry_record(const TelemetrySnapshot& snapshot,
                                                   uint8_t* out_buf,
                                                   std::size_t buf_size) {
    if (out_buf == nullptr || buf_size < kRecordSize) {
        return 0u;
    }

    static_assert(hal::kAdcChannelCount == 10u, "Registro espera 10 canais ADC");
    static_assert(VehicleCanState::kWheelCount == 4u, "Registro espera 4 rodas");

    std::size_t offset = 0u;

    const uint8_t status_flags = static_cast<uint8_t>(
        (snapshot.rtc_synchronized ? binlog::kRecordFlagRtcSynced : 0u) |
        (snapshot.gps_has_fix ? binlog::kRecordFlagGpsHasFix : 0u));

    // 0..31: Cabeçalho do registro e metadados temporais/diagnóstico
    write_u16_le(out_buf, offset, binlog::kRecordSyncWord);
    write_u8(out_buf, offset, binlog::kRecordTypeTelemetry);
    write_u8(out_buf, offset, status_flags);
    write_u32_le(out_buf, offset, snapshot.sample_seq);
    write_u64_le(out_buf, offset, snapshot.monotonic_ms);
    write_u64_le(out_buf, offset, snapshot.epoch_ms);
    write_u16_le(out_buf, offset, snapshot.lap_beacon);
    write_u16_le(out_buf, offset, snapshot.adc_valid_mask);
    write_u8(out_buf, offset, snapshot.can_node_online_mask);
    write_u8(out_buf, offset, snapshot.gps_satellites);
    write_u16_le(out_buf, offset, snapshot.imu.status_flags);

    // 32..63: GPS Neo-8M
    write_f64_le(out_buf, offset, snapshot.gps_latitude_deg);
    write_f64_le(out_buf, offset, snapshot.gps_longitude_deg);
    write_f32_le(out_buf, offset, snapshot.gps_altitude_m);
    write_f32_le(out_buf, offset, snapshot.gps_speed_kmh);
    write_f32_le(out_buf, offset, snapshot.gps_speed_mps);
    write_f32_le(out_buf, offset, snapshot.gps_course_deg);

    // 64..103: 10 Canais Analógicos Locais (ADC1)
    for (std::size_t i = 0u; i < hal::kAdcChannelCount; ++i) {
        write_f32_le(out_buf, offset, snapshot.adc_physical[i]);
    }

    // 104..119: Nó CAN 0x100 (Freio e Direção)
    write_f32_le(out_buf, offset, snapshot.front_controls.steering_angle_deg);
    write_f32_le(out_buf, offset, snapshot.front_controls.brake_pressure_front_bar);
    write_f32_le(out_buf, offset, snapshot.front_controls.brake_pressure_rear_bar);
    write_f32_le(out_buf, offset, snapshot.front_controls.brake_pedal_pos_pct);

    // 120..183: 4 Nós CAN de Roda (0x200 FL, 0x201 FR, 0x202 RL, 0x203 RR)
    for (std::size_t w = 0u; w < VehicleCanState::kWheelCount; ++w) {
        const auto& wheel = snapshot.wheels[w];
        write_f32_le(out_buf, offset, wheel.damper_pos_mm);
        write_f32_le(out_buf, offset, wheel.wheel_speed_kmh);
        write_f32_le(out_buf, offset, wheel.load_cell_n);
        write_f32_le(out_buf, offset, wheel.brake_disc_temp_c);
    }

    // 184..207: Nó CAN IMU WIT (0x300 Accel + 0x301 Gyro)
    write_f32_le(out_buf, offset, snapshot.imu.accel_x_mps2);
    write_f32_le(out_buf, offset, snapshot.imu.accel_y_mps2);
    write_f32_le(out_buf, offset, snapshot.imu.accel_z_mps2);
    write_f32_le(out_buf, offset, snapshot.imu.gyro_x_dps);
    write_f32_le(out_buf, offset, snapshot.imu.gyro_y_dps);
    write_f32_le(out_buf, offset, snapshot.imu.gyro_z_dps);

    // 208..249: ECU FuelTech FTCAN 2.0 (0x600..0x602 / 0x14080600..0x14080602)
    write_f32_le(out_buf, offset, snapshot.ecu.tps_pct);
    write_f32_le(out_buf, offset, snapshot.ecu.map_bar);
    write_f32_le(out_buf, offset, snapshot.ecu.air_temp_c);
    write_f32_le(out_buf, offset, snapshot.ecu.engine_temp_c);
    write_f32_le(out_buf, offset, snapshot.ecu.oil_pressure_bar);
    write_f32_le(out_buf, offset, snapshot.ecu.fuel_pressure_bar);
    write_f32_le(out_buf, offset, snapshot.ecu.water_pressure_bar);
    write_f32_le(out_buf, offset, snapshot.ecu.exhaust_o2_lambda);
    write_f32_le(out_buf, offset, snapshot.ecu.oil_temp_c);
    write_u16_le(out_buf, offset, snapshot.ecu.rpm);
    write_i16_le(out_buf, offset, snapshot.ecu.gear);
    write_u16_le(out_buf, offset, snapshot.ecu.pit_limit_status);

    // 250..251: Reservado
    write_u16_le(out_buf, offset, 0u);

    // 252..255: CRC-32 IEEE 802.3 sobre bytes 0..251
    const uint32_t crc = compute_crc32(out_buf, offset);
    write_u32_le(out_buf, offset, crc);

    return (offset == kRecordSize) ? kRecordSize : 0u;
}

bool LoggerService::build_session_file_path(std::string_view session_ts) {
    if (session_ts.empty()) {
        return false;
    }

    const std::size_t mount_len = std::strlen(mount_point_buf_.data());
    const bool needs_slash = (mount_len > 0u && mount_point_buf_[mount_len - 1u] != '/');
    constexpr std::string_view kPrefix{"LOG_"};
    constexpr std::string_view kExt = binlog::kFileExtension;

    const std::size_t total_len =
        mount_len + (needs_slash ? 1u : 0u) + kPrefix.size() + session_ts.size() + kExt.size();
    if (total_len >= current_file_path_.size()) {
        return false;
    }

    std::size_t pos = 0u;
    std::memcpy(current_file_path_.data() + pos, mount_point_buf_.data(), mount_len);
    pos += mount_len;

    if (needs_slash) {
        current_file_path_[pos++] = '/';
    }

    std::memcpy(current_file_path_.data() + pos, kPrefix.data(), kPrefix.size());
    pos += kPrefix.size();

    std::memcpy(current_file_path_.data() + pos, session_ts.data(), session_ts.size());
    pos += session_ts.size();

    std::memcpy(current_file_path_.data() + pos, kExt.data(), kExt.size());
    pos += kExt.size();

    current_file_path_[pos] = '\0';
    return true;
}

bool LoggerService::flush_block() {
    if (block_len_ == 0u) {
        return true;
    }
    if (!storage_hal_.write(reinterpret_cast<const char*>(block_buf_.data()), block_len_)) {
        return false;
    }
    bytes_written_ += static_cast<uint64_t>(block_len_);
    block_len_ = 0u;
    return true;
}

} // namespace datalogger::bsw
