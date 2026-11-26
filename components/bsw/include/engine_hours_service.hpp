#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "i_nvm_hal.hpp"

namespace datalogger::bsw {

/**
 * @brief Parâmetros operacionais do Horímetro de Motor (Padrão Motorsport / MoTeC).
 */
struct EngineHoursConfig {
    hal::NvmConfig nvm{};
    const char* key{"eng_sec_tot"};           // Chave NVS (<= 15 chars): segundos totais acumulados
    uint16_t rpm_run_threshold{600u};         // Engine Run Threshold: conta quando ECU online e RPM > 600
    uint64_t save_interval_ms{60000ULL};      // Checkpoint periódico na NVS a cada 1 min (60 s) rodando
    uint64_t max_step_delta_ms{5000ULL};      // Limite de segurança por passo de integração
};

/**
 * @brief BSW (Basic Software Service) - Horímetro Permanente de Motor (*Engine Run Time Total*).
 *
 * Contabiliza o tempo acumulado de motor em funcionamento (`ecu_online && ecu_rpm > 600`)
 * seguindo a estratégia padrão de ECUs/Loggers MoTeC:
 * - Acumulação contínua em RAM sem alocação dinâmica (zero heap);
 * - Checkpoint periódico na NVS a cada `1 minuto` (`60 s`) de motor em funcionamento;
 * - Gravação imediata na NVS na transição de parada do motor (preservando segundos parciais);
 * - Formatação estática `HH:MM` (ex.: `"00:00"`, `"12:34"`, `"125:08"`) para telemetria e logs.
 */
class EngineHoursService {
public:
    static constexpr std::size_t kMinFormatBufSize = 16u;

    explicit EngineHoursService(hal::INvmHal& nvm_hal);

    /**
     * @brief Inicializa a NVS e restaura o contador acumulado de horas/segundos do motor.
     */
    bool initialize(const EngineHoursConfig& config = EngineHoursConfig{});

    /**
     * @brief Atualiza o integrador de tempo de motor ligado e dispara checkpoints NVS quando devido.
     */
    void update(uint64_t now_ms, uint16_t ecu_rpm, bool ecu_online);

    /**
     * @brief Define manualmente o total de segundos do motor (ex.: motor já rodado em dinamômetro) e grava na NVS.
     */
    bool set_total_seconds(uint32_t total_seconds);

    /**
     * @brief Zera o horímetro do motor (ex.: após revisão/retífica completa) e grava na NVS.
     */
    bool reset_hours() { return set_total_seconds(0u); }

    /**
     * @brief Formata o tempo acumulado em `HH:MM` sem uso de heap (ex.: `"02:05"`, `"125:08"`).
     * @return Quantidade de caracteres gravados (sem contar `'\0'`), ou `0` se o buffer for pequeno.
     */
    std::size_t format_hhmm(char* out_buf, std::size_t buf_size) const;

    bool is_initialized() const { return initialized_; }
    bool is_nvm_healthy() const { return nvm_healthy_; }
    bool is_engine_running() const { return engine_running_; }

    uint32_t total_seconds() const;
    uint32_t total_minutes() const { return total_seconds() / 60u; }
    uint32_t hours_part() const { return total_seconds() / 3600u; }
    uint8_t minutes_part() const { return static_cast<uint8_t>((total_seconds() % 3600u) / 60u); }
    uint8_t seconds_part() const { return static_cast<uint8_t>(total_seconds() % 60u); }

    uint32_t persisted_seconds() const { return persisted_seconds_; }
    uint32_t save_count() const { return save_count_; }
    uint32_t nvm_error_count() const { return nvm_error_count_; }

private:
    bool persist_total_seconds(uint32_t seconds_to_save);

    hal::INvmHal& nvm_hal_;
    EngineHoursConfig config_{};
    std::array<char, 16> key_buf_{};
    bool initialized_{false};
    bool nvm_initialized_{false};
    bool nvm_healthy_{false};
    bool engine_running_{false};
    uint64_t last_tick_ms_{0ULL};
    uint64_t total_runtime_ms_{0ULL};
    uint64_t unsaved_run_ms_{0ULL};
    uint32_t persisted_seconds_{0u};
    uint32_t save_count_{0u};
    uint32_t nvm_error_count_{0u};
};

} // namespace datalogger::bsw
