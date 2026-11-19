#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "driver/i2c_master.h"
#include "i_rtc_hal.hpp"

namespace datalogger::lld {

/**
 * @brief LLD (Low-Level Driver / MCAL) do RTC externo DS3231SN# via barramento I2C Master (ESP-IDF v5.2+).
 *
 * Implementa a interface `hal::IRtcHal` utilizando `driver/i2c_master.h` para comunicação com o CI
 * DS3231SN# (endereço 7-bit 0x68 @ 400 kHz por padrão) e `esp_timer_get_time()` para base
 * monotônica de 1 ms. Realiza leitura em rajada (`0x00..0x0F`) para evitar tearing de rollover
 * e preserva o flag OSF na leitura, limpando-o apenas em `write_datetime()`.
 */
class Ds3231I2cLld : public hal::IRtcHal {
public:
    static constexpr uint8_t kRegSeconds      = 0x00u;
    static constexpr uint8_t kRegMinutes      = 0x01u;
    static constexpr uint8_t kRegHours        = 0x02u;
    static constexpr uint8_t kRegDayOfWeek    = 0x03u;
    static constexpr uint8_t kRegDate         = 0x04u;
    static constexpr uint8_t kRegMonthCentury = 0x05u;
    static constexpr uint8_t kRegYear         = 0x06u;
    static constexpr uint8_t kRegControl      = 0x0Eu;
    static constexpr uint8_t kRegStatus       = 0x0Fu;

    static constexpr std::size_t kTimekeepingRegCount = 7u;
    static constexpr std::size_t kFullReadRegCount    = 16u;

    static constexpr uint8_t kHours12hModeMask = 0x40u;
    static constexpr uint8_t kHoursPmMask      = 0x20u;
    static constexpr uint8_t kCenturyBitMask   = 0x80u;
    static constexpr uint8_t kControlEoscMask  = 0x80u;
    static constexpr uint8_t kStatusOsfMask    = 0x80u;

    Ds3231I2cLld() = default;
    ~Ds3231I2cLld() override;

    /**
     * @brief Valida a configuração I2C e inicializa o barramento master e dispositivo DS3231SN#.
     */
    bool init(const hal::RtcI2cConfig& config) override;

    /**
     * @brief Lê em rajada os registradores `0x00..0x0F` sem limpar o bit OSF.
     */
    bool read_datetime(hal::RtcDateTime& out_dt, hal::RtcDeviceStatus& out_status) override;

    /**
     * @brief Grava data/hora em modo 24h BCD (`0x00..0x06`), habilita EOSC=0 (`0x0E`) e limpa OSF (`0x0F`).
     */
    bool write_datetime(const hal::RtcDateTime& dt) override;

    /**
     * @brief Retorna o contador monotônico desde o boot em milissegundos via `esp_timer_get_time()`.
     */
    uint64_t get_monotonic_ms() const override;

    bool is_initialized() const { return initialized_; }

    /**
     * @brief Converte um byte BCD (0x00..0x99) para decimal validando ambos os nibbles (0..9).
     */
    static bool bcd_to_dec(uint8_t bcd, uint8_t& out_dec);

    /**
     * @brief Converte um valor decimal (0..99) para BCD.
     */
    static bool dec_to_bcd(uint8_t dec, uint8_t& out_bcd);

    /**
     * @brief Codifica `RtcDateTime` para os 7 registradores de tempo do DS3231 (`0x00..0x06`) em modo 24h.
     */
    static bool encode_timekeeping_registers(
        const hal::RtcDateTime& dt,
        std::array<uint8_t, kTimekeepingRegCount>& out_regs);

    /**
     * @brief Decodifica os 7 registradores de tempo (`0x00..0x06`), suportando modos 24h, 12h AM/PM e bit Century.
     */
    static bool decode_timekeeping_registers(
        const std::array<uint8_t, kTimekeepingRegCount>& regs,
        hal::RtcDateTime& out_dt);

    /**
     * @brief Retorna true se o bit OSF (bit 7 do registrador 0x0F) estiver ativo.
     */
    static bool is_oscillator_stopped(uint8_t status_reg);

    /**
     * @brief Retorna o byte de status com o bit OSF (bit 7) limpo, preservando os demais bits.
     */
    static uint8_t clear_oscillator_stopped_bit(uint8_t status_reg);

    /**
     * @brief Retorna o byte de controle com o bit /EOSC (bit 7) limpo (=0, oscilador habilitado).
     */
    static uint8_t enable_oscillator_control_bit(uint8_t control_reg);

private:
    void deinit();
    bool read_register(uint8_t reg_addr, uint8_t& out_value);
    bool write_register(uint8_t reg_addr, uint8_t value);

    i2c_master_bus_handle_t bus_handle_{nullptr};
    i2c_master_dev_handle_t dev_handle_{nullptr};
    hal::RtcI2cConfig config_{};
    bool initialized_{false};
};

} // namespace datalogger::lld
