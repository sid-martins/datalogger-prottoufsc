#include "ds3231_i2c_lld.hpp"

#include <algorithm>
#include "esp_log.h"
#include "esp_timer.h"

namespace datalogger::lld {

namespace {

constexpr const char* kTag = "DS3231_I2C_LLD";

bool is_valid_gpio_pin(gpio_num_t pin) {
    const int p = static_cast<int>(pin);
    if (p < 0 || p >= static_cast<int>(GPIO_NUM_MAX)) {
        return false;
    }
    // GPIOs 26..32 são reservados à Flash SPI interna do ESP32-S3
    if (p >= 26 && p <= 32) {
        return false;
    }
    return true;
}

} // namespace

Ds3231I2cLld::~Ds3231I2cLld() {
    deinit();
}

void Ds3231I2cLld::deinit() {
    if (dev_handle_ != nullptr) {
        i2c_master_bus_rm_device(dev_handle_);
        dev_handle_ = nullptr;
    }
    if (bus_handle_ != nullptr) {
        i2c_del_master_bus(bus_handle_);
        bus_handle_ = nullptr;
    }
    initialized_ = false;
}

bool Ds3231I2cLld::init(const hal::RtcI2cConfig& config) {
    deinit();

    if (!is_valid_gpio_pin(config.sda_pin) ||
        !is_valid_gpio_pin(config.scl_pin) ||
        config.sda_pin == config.scl_pin) {
        ESP_LOGE(kTag, "Pinos I2C invalidos (SDA=%d, SCL=%d)",
                 static_cast<int>(config.sda_pin),
                 static_cast<int>(config.scl_pin));
        return false;
    }

    if (config.clk_speed_hz == 0u || config.device_address > 0x7Fu) {
        ESP_LOGE(kTag, "Parametros I2C invalidos (freq=%lu, addr=0x%02X)",
                 static_cast<unsigned long>(config.clk_speed_hz),
                 static_cast<unsigned>(config.device_address));
        return false;
    }

    if (config.timeout_ms <= 0) {
        ESP_LOGE(kTag, "Timeout I2C invalido (%d ms)", static_cast<int>(config.timeout_ms));
        return false;
    }

    if (static_cast<int>(config.i2c_port) >= static_cast<int>(I2C_NUM_MAX)) {
        ESP_LOGE(kTag, "Porta I2C invalida (%u)", static_cast<unsigned>(config.i2c_port));
        return false;
    }

    i2c_master_bus_config_t bus_cfg{};
    bus_cfg.i2c_port = static_cast<i2c_port_num_t>(config.i2c_port);
    bus_cfg.sda_io_num = config.sda_pin;
    bus_cfg.scl_io_num = config.scl_pin;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7u;
    bus_cfg.intr_priority = 0;
    bus_cfg.trans_queue_depth = 0u;
    bus_cfg.flags.enable_internal_pullup = 1u;

    if (i2c_new_master_bus(&bus_cfg, &bus_handle_) != ESP_OK || bus_handle_ == nullptr) {
        ESP_LOGE(kTag, "Falha ao criar barramento I2C master");
        deinit();
        return false;
    }

    i2c_device_config_t dev_cfg{};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = config.device_address;
    dev_cfg.scl_speed_hz = config.clk_speed_hz;
    dev_cfg.scl_wait_us = 0u;
    dev_cfg.flags.disable_ack_check = 0u;

    if (i2c_master_bus_add_device(bus_handle_, &dev_cfg, &dev_handle_) != ESP_OK || dev_handle_ == nullptr) {
        ESP_LOGE(kTag, "Falha ao registrar dispositivo DS3231 no barramento I2C");
        deinit();
        return false;
    }

    config_ = config;
    initialized_ = true;
    ESP_LOGI(kTag, "DS3231 I2C inicializado (porta=%d, SDA=%d, SCL=%d, addr=0x%02X, %lu Hz)",
             static_cast<int>(config_.i2c_port),
             static_cast<int>(config_.sda_pin),
             static_cast<int>(config_.scl_pin),
             static_cast<unsigned>(config_.device_address),
             static_cast<unsigned long>(config_.clk_speed_hz));
    return true;
}

bool Ds3231I2cLld::read_datetime(hal::RtcDateTime& out_dt, hal::RtcDeviceStatus& out_status) {
    if (!initialized_ || dev_handle_ == nullptr) {
        return false;
    }

    const uint8_t start_reg = kRegSeconds;
    std::array<uint8_t, kFullReadRegCount> raw{};

    const esp_err_t err = i2c_master_transmit_receive(
        dev_handle_,
        &start_reg,
        1u,
        raw.data(),
        raw.size(),
        static_cast<int>(config_.timeout_ms));
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Falha I2C ao ler DS3231 (err=%d)", static_cast<int>(err));
        return false;
    }

    // Preserva o bit OSF na leitura (limpeza ocorre apenas em write_datetime)
    out_status.oscillator_stopped = is_oscillator_stopped(raw[kRegStatus]);

    std::array<uint8_t, kTimekeepingRegCount> time_regs{};
    std::copy_n(raw.begin(), kTimekeepingRegCount, time_regs.begin());
    return decode_timekeeping_registers(time_regs, out_dt);
}

bool Ds3231I2cLld::write_datetime(const hal::RtcDateTime& dt) {
    if (!initialized_ || dev_handle_ == nullptr) {
        return false;
    }

    std::array<uint8_t, kTimekeepingRegCount> time_regs{};
    if (!encode_timekeeping_registers(dt, time_regs)) {
        return false;
    }

    std::array<uint8_t, 1u + kTimekeepingRegCount> tx_buf{};
    tx_buf[0] = kRegSeconds;
    std::copy(time_regs.begin(), time_regs.end(), tx_buf.begin() + 1u);

    const esp_err_t err = i2c_master_transmit(
        dev_handle_,
        tx_buf.data(),
        tx_buf.size(),
        static_cast<int>(config_.timeout_ms));
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Falha I2C ao gravar data/hora no DS3231 (err=%d)", static_cast<int>(err));
        return false;
    }

    uint8_t ctrl = 0u;
    if (!read_register(kRegControl, ctrl)) {
        return false;
    }
    const uint8_t next_ctrl = enable_oscillator_control_bit(ctrl);
    if (next_ctrl != ctrl && !write_register(kRegControl, next_ctrl)) {
        return false;
    }

    uint8_t status = 0u;
    if (!read_register(kRegStatus, status)) {
        return false;
    }
    if (is_oscillator_stopped(status)) {
        if (!write_register(kRegStatus, clear_oscillator_stopped_bit(status))) {
            return false;
        }
    }

    return true;
}

uint64_t Ds3231I2cLld::get_monotonic_ms() const {
    const int64_t time_us = esp_timer_get_time();
    if (time_us <= 0) {
        return 0ULL;
    }
    return static_cast<uint64_t>(time_us) / 1000ULL;
}

bool Ds3231I2cLld::read_register(uint8_t reg_addr, uint8_t& out_value) {
    if (dev_handle_ == nullptr) {
        return false;
    }
    const esp_err_t err = i2c_master_transmit_receive(
        dev_handle_,
        &reg_addr,
        1u,
        &out_value,
        1u,
        static_cast<int>(config_.timeout_ms));
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Falha I2C ao ler registrador 0x%02X (err=%d)",
                 static_cast<unsigned>(reg_addr),
                 static_cast<int>(err));
        return false;
    }
    return true;
}

bool Ds3231I2cLld::write_register(uint8_t reg_addr, uint8_t value) {
    if (dev_handle_ == nullptr) {
        return false;
    }
    const std::array<uint8_t, 2u> tx_buf{reg_addr, value};
    const esp_err_t err = i2c_master_transmit(
        dev_handle_,
        tx_buf.data(),
        tx_buf.size(),
        static_cast<int>(config_.timeout_ms));
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Falha I2C ao gravar registrador 0x%02X (err=%d)",
                 static_cast<unsigned>(reg_addr),
                 static_cast<int>(err));
        return false;
    }
    return true;
}

bool Ds3231I2cLld::bcd_to_dec(uint8_t bcd, uint8_t& out_dec) {
    const uint8_t tens = static_cast<uint8_t>((bcd >> 4u) & 0x0Fu);
    const uint8_t ones = static_cast<uint8_t>(bcd & 0x0Fu);
    if (tens > 9u || ones > 9u) {
        return false;
    }
    out_dec = static_cast<uint8_t>((tens * 10u) + ones);
    return true;
}

bool Ds3231I2cLld::dec_to_bcd(uint8_t dec, uint8_t& out_bcd) {
    if (dec > 99u) {
        return false;
    }
    const uint8_t tens = static_cast<uint8_t>(dec / 10u);
    const uint8_t ones = static_cast<uint8_t>(dec % 10u);
    out_bcd = static_cast<uint8_t>((tens << 4u) | ones);
    return true;
}

bool Ds3231I2cLld::encode_timekeeping_registers(
    const hal::RtcDateTime& dt,
    std::array<uint8_t, kTimekeepingRegCount>& out_regs) {
    if (dt.year < 2000u || dt.year > 2099u ||
        dt.month < 1u || dt.month > 12u ||
        dt.day < 1u || dt.day > 31u ||
        dt.day_of_week < 1u || dt.day_of_week > 7u ||
        dt.hour > 23u || dt.minute > 59u || dt.second > 59u) {
        return false;
    }

    uint8_t bcd_sec = 0u;
    uint8_t bcd_min = 0u;
    uint8_t bcd_hour = 0u;
    uint8_t bcd_dow = 0u;
    uint8_t bcd_date = 0u;
    uint8_t bcd_month = 0u;
    uint8_t bcd_year = 0u;

    const uint8_t year_offset = static_cast<uint8_t>(dt.year - 2000u);
    if (!dec_to_bcd(dt.second, bcd_sec) ||
        !dec_to_bcd(dt.minute, bcd_min) ||
        !dec_to_bcd(dt.hour, bcd_hour) ||
        !dec_to_bcd(dt.day_of_week, bcd_dow) ||
        !dec_to_bcd(dt.day, bcd_date) ||
        !dec_to_bcd(dt.month, bcd_month) ||
        !dec_to_bcd(year_offset, bcd_year)) {
        return false;
    }

    out_regs[kRegSeconds]      = static_cast<uint8_t>(bcd_sec & 0x7Fu);
    out_regs[kRegMinutes]      = static_cast<uint8_t>(bcd_min & 0x7Fu);
    out_regs[kRegHours]        = static_cast<uint8_t>(bcd_hour & 0x3Fu); // Modo 24h (bit 6 = 0)
    out_regs[kRegDayOfWeek]    = static_cast<uint8_t>(bcd_dow & 0x07u);
    out_regs[kRegDate]         = static_cast<uint8_t>(bcd_date & 0x3Fu);
    out_regs[kRegMonthCentury] = static_cast<uint8_t>(bcd_month & 0x1Fu); // Century = 0
    out_regs[kRegYear]         = bcd_year;
    return true;
}

bool Ds3231I2cLld::decode_timekeeping_registers(
    const std::array<uint8_t, kTimekeepingRegCount>& regs,
    hal::RtcDateTime& out_dt) {
    uint8_t second = 0u;
    uint8_t minute = 0u;
    if (!bcd_to_dec(static_cast<uint8_t>(regs[kRegSeconds] & 0x7Fu), second) || second > 59u) {
        return false;
    }
    if (!bcd_to_dec(static_cast<uint8_t>(regs[kRegMinutes] & 0x7Fu), minute) || minute > 59u) {
        return false;
    }

    uint8_t hour = 0u;
    if ((regs[kRegHours] & kHours12hModeMask) == 0u) {
        if (!bcd_to_dec(static_cast<uint8_t>(regs[kRegHours] & 0x3Fu), hour) || hour > 23u) {
            return false;
        }
    } else {
        uint8_t h12 = 0u;
        if (!bcd_to_dec(static_cast<uint8_t>(regs[kRegHours] & 0x1Fu), h12) ||
            h12 < 1u || h12 > 12u) {
            return false;
        }
        const bool is_pm = (regs[kRegHours] & kHoursPmMask) != 0u;
        if (h12 == 12u) {
            hour = is_pm ? 12u : 0u;
        } else {
            hour = is_pm ? static_cast<uint8_t>(h12 + 12u) : h12;
        }
    }

    const uint8_t dow = static_cast<uint8_t>(regs[kRegDayOfWeek] & 0x07u);
    if (dow < 1u || dow > 7u) {
        return false;
    }

    uint8_t day = 0u;
    if (!bcd_to_dec(static_cast<uint8_t>(regs[kRegDate] & 0x3Fu), day) ||
        day < 1u || day > 31u) {
        return false;
    }

    const bool century = (regs[kRegMonthCentury] & kCenturyBitMask) != 0u;
    uint8_t month = 0u;
    if (!bcd_to_dec(static_cast<uint8_t>(regs[kRegMonthCentury] & 0x1Fu), month) ||
        month < 1u || month > 12u) {
        return false;
    }

    uint8_t year_yy = 0u;
    if (!bcd_to_dec(regs[kRegYear], year_yy)) {
        return false;
    }

    const uint16_t base_year = century ? 2100u : 2000u;
    out_dt.year        = static_cast<uint16_t>(base_year + static_cast<uint16_t>(year_yy));
    out_dt.month       = month;
    out_dt.day         = day;
    out_dt.day_of_week = dow;
    out_dt.hour        = hour;
    out_dt.minute      = minute;
    out_dt.second      = second;
    return true;
}

bool Ds3231I2cLld::is_oscillator_stopped(uint8_t status_reg) {
    return (status_reg & kStatusOsfMask) != 0u;
}

uint8_t Ds3231I2cLld::clear_oscillator_stopped_bit(uint8_t status_reg) {
    return static_cast<uint8_t>(status_reg & static_cast<uint8_t>(~kStatusOsfMask));
}

uint8_t Ds3231I2cLld::enable_oscillator_control_bit(uint8_t control_reg) {
    return static_cast<uint8_t>(control_reg & static_cast<uint8_t>(~kControlEoscMask));
}

} // namespace datalogger::lld
