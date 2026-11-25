#pragma once

#include <cstdint>
#include "bsp_pins.h"

namespace datalogger::hal {

/**
 * @brief Configuração do barramento I2C para comunicação com o RTC externo DS3231SN#.
 */
struct RtcI2cConfig {
    gpio_num_t sda_pin{PIN_I2C_SDA};
    gpio_num_t scl_pin{PIN_I2C_SCL};
    uint32_t clk_speed_hz{I2C_MASTER_FREQ_HZ};
    uint8_t device_address{RTC_DS3231_I2C_ADDR};
    int32_t timeout_ms{50};
    uint8_t i2c_port{0};
};

/**
 * @brief Representação de data e hora do calendário UTC no domínio do RTC (2000..2099).
 *
 * `day_of_week` segue estritamente a convenção do DS3231SN# e ISO 8601 no intervalo `1..7`.
 */
struct RtcDateTime {
    uint16_t year{2000};
    uint8_t month{1};
    uint8_t day{1};
    uint8_t day_of_week{1};
    uint8_t hour{0};
    uint8_t minute{0};
    uint8_t second{0};
};

/**
 * @brief Sinalizadores de diagnóstico do dispositivo RTC físico.
 */
struct RtcDeviceStatus {
    bool oscillator_stopped{false};
};

/**
 * @brief Interface HAL (Hardware Abstraction Layer) para acesso ao RTC e relógio monotônico de 1 ms.
 */
class IRtcHal {
public:
    virtual ~IRtcHal() = default;

    /**
     * @brief Inicializa a interface física de comunicação com o RTC.
     */
    virtual bool init(const RtcI2cConfig& config) = 0;

    /**
     * @brief Lê data/hora atuais e o estado do oscilador (OSF) sem limpar o flag OSF.
     */
    virtual bool read_datetime(RtcDateTime& out_dt, RtcDeviceStatus& out_status) = 0;

    /**
     * @brief Grava data/hora válidas no RTC, garante oscilador habilitado e limpa o flag OSF.
     */
    virtual bool write_datetime(const RtcDateTime& dt) = 0;

    /**
     * @brief Retorna o tempo monotônico desde o boot em milissegundos (resolução de 1 ms).
     */
    virtual uint64_t get_monotonic_ms() const = 0;
};

} // namespace datalogger::hal
