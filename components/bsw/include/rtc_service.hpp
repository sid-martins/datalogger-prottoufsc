#pragma once

#include <cstddef>
#include <cstdint>
#include "i_rtc_hal.hpp"

namespace datalogger::bsw {

/**
 * @brief Estado de sincronização temporal do serviço de RTC.
 */
enum class RtcSyncStatus : uint8_t {
    kUninitialized = 0,
    kSynchronized,
    kHalInitFailed,
    kI2cReadFailed,
    kOscillatorStopped,
    kInvalidCalendar
};

/**
 * @brief Carimbo temporal combinado (relógio monotônico de 1 ms + epoch UTC ou fallback monotônico).
 */
struct RtcTimestamp {
    uint64_t monotonic_ms{0};
    uint64_t epoch_ms{0};
    bool synchronized{false};
};

/**
 * @brief BSW (Basic Software Service) - Serviço de Sincronização Temporal e RTC DS3231SN#.
 *
 * Responsável por inicializar o hardware de RTC via `hal::IRtcHal`, validar o calendário
 * gregoriano (2000..2099), ancorar o tempo UTC em uma base monotônica de 1 ms (midpoint anchor
 * com erro de alinhamento <= 50 ms e resolução de 1 ms), formatar timestamps de sessão sem
 * alocação de heap e operar em modo degradado monotônico em caso de falha de I2C, perda de
 * oscilador (OSF) ou calendário inválido.
 */
class RtcService {
public:
    static constexpr uint16_t kMinYear = 2000u;
    static constexpr uint16_t kMaxYear = 2099u;
    static constexpr std::size_t kMinSessionTimestampBufSize = 32u;

    explicit RtcService(hal::IRtcHal& rtc_hal);

    /**
     * @brief Inicializa o HAL do RTC e executa a primeira sincronização com a âncora monotônica.
     */
    bool initialize(const hal::RtcI2cConfig& config = hal::RtcI2cConfig{});

    /**
     * @brief Re-sincroniza a âncora temporal lendo novamente data/hora e flag OSF do RTC.
     */
    bool resync();

    /**
     * @brief Ajusta data/hora válidas no RTC físico e atualiza a âncora monotônica se o HAL estiver inicializado.
     */
    bool set_datetime(const hal::RtcDateTime& dt);

    /**
     * @brief Captura um snapshot temporal atômico (monotônico 1 ms + epoch/fallback + flag synchronized).
     */
    RtcTimestamp sample_timestamp() const;

    /**
     * @brief Retorna o timestamp atual em ms (epoch UTC se sincronizado, ou monotônico em modo degradado).
     */
    uint64_t now_epoch_ms() const;

    /**
     * @brief Projeta um timestamp monotônico arbitrário para epoch ms (ou retorna o próprio valor se degradado).
     */
    uint64_t monotonic_to_epoch_ms(uint64_t monotonic_ms) const;

    /**
     * @brief Obtém a data/hora UTC atual; retorna false quando não sincronizado (modo degradado).
     */
    bool now_datetime(hal::RtcDateTime& out_dt) const;

    /**
     * @brief Formata um identificador de sessão sem heap (`YYYYMMDD_HHMMSS` ou `BOOT_<ms>ms`).
     */
    std::size_t format_session_timestamp(char* out_buf, std::size_t buf_size) const;

    bool is_synchronized() const { return synchronized_; }
    bool is_degraded() const { return (status_ != RtcSyncStatus::kUninitialized) && !synchronized_; }
    RtcSyncStatus status() const { return status_; }
    uint32_t sync_count() const { return sync_count_; }
    uint32_t sync_error_count() const { return sync_error_count_; }
    uint64_t anchor_monotonic_ms() const { return anchor_monotonic_ms_; }
    uint64_t anchor_epoch_ms() const { return anchor_epoch_ms_; }

    /**
     * @brief Valida data/hora no intervalo 2000..2099, incluindo anos bissextos e dia da semana 1..7.
     */
    static bool is_valid_datetime(const hal::RtcDateTime& dt);

    /**
     * @brief Retorna true se o ano gregoriano for bissexto.
     */
    static bool is_leap_year(uint16_t year);

    /**
     * @brief Retorna a quantidade de dias do mês (1..12) considerando ano bissexto, ou 0 se mês inválido.
     */
    static uint8_t days_in_month(uint16_t year, uint8_t month);

    /**
     * @brief Converte `RtcDateTime` UTC válido em milissegundos desde 1970-01-01T00:00:00Z (retorna 0 se inválido).
     */
    static uint64_t datetime_to_epoch_ms(const hal::RtcDateTime& dt);

    /**
     * @brief Converte milissegundos desde 1970-01-01T00:00:00Z para `RtcDateTime` UTC (sem uso de TZ/heap).
     */
    static hal::RtcDateTime epoch_ms_to_datetime(uint64_t epoch_ms);

private:
    bool sync_from_hal();

    hal::IRtcHal& rtc_hal_;
    bool hal_initialized_{false};
    bool synchronized_{false};
    RtcSyncStatus status_{RtcSyncStatus::kUninitialized};
    uint64_t anchor_monotonic_ms_{0};
    uint64_t anchor_epoch_ms_{0};
    uint32_t sync_count_{0};
    uint32_t sync_error_count_{0};
};

} // namespace datalogger::bsw
