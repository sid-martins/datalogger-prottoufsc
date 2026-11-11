#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include "i_rtos_queue.hpp"
#include "i_sensor.hpp"
#include "i_storage.hpp"

namespace datalogger {

/**
 * @brief Serviço central do Datalogger.
 *
 * Projetado com Injeção de Dependência (ISensor, IStorage, IRtosQueue) para que
 * os passos executados pelas Tasks do FreeRTOS (Task de Aquisição e Task de Escrita)
 * possam ser 100% testados via TDD com Google Test / Google Mock no computador Host.
 */
class DataloggerService {
public:
    static constexpr float kMinValidTempC = -40.0f;
    static constexpr float kMaxValidTempC = 125.0f;

    DataloggerService(hal::ISensor& sensor,
                      hal::IStorage& storage,
                      hal::IRtosQueue<hal::SensorReading>& sample_queue,
                      std::string_view log_file_path = "/storage/datalog.csv",
                      uint32_t sync_every_n_samples = 5);

    /**
     * @brief Inicializa o sensor e monta o sistema de arquivos.
     * @return true se ambos inicializaram com sucesso.
     */
    bool initialize();

    /**
     * @brief Passo executado pela Task Produtora (FreeRTOS Acquisition Task).
     * Lê uma amostra do sensor, valida os limites físicos e envia para a fila do FreeRTOS.
     * @param queue_timeout_ms Timeout para envio na fila.
     * @return true se uma amostra válida foi lida e enfileirada com sucesso.
     */
    bool acquisition_step(uint32_t queue_timeout_ms = 50);

    /**
     * @brief Passo executado pela Task Consumidora (FreeRTOS Storage Task).
     * Retira uma amostra da fila do FreeRTOS, formata em CSV e grava no armazenamento.
     * Executa sync() periodicamente a cada `sync_every_n_samples` amostras gravadas.
     * @param queue_timeout_ms Timeout para aguardar amostra na fila.
     * @return true se uma amostra foi consumida e gravada com sucesso.
     */
    bool storage_step(uint32_t queue_timeout_ms = 100);

    /**
     * @brief Formata uma leitura de sensor como linha CSV:
     * "timestamp_ms,temperature_c,humidity_pct,battery_mv"
     */
    static std::string format_csv_line(const hal::SensorReading& reading);

    uint32_t samples_logged() const { return samples_logged_; }
    uint32_t read_errors() const { return read_errors_; }
    uint32_t dropped_samples() const { return dropped_samples_; }

private:
    static bool is_reading_valid(const hal::SensorReading& reading);

    hal::ISensor& sensor_;
    hal::IStorage& storage_;
    hal::IRtosQueue<hal::SensorReading>& sample_queue_;
    std::string log_file_path_;
    uint32_t sync_every_n_samples_;

    uint32_t samples_logged_{0};
    uint32_t read_errors_{0};
    uint32_t dropped_samples_{0};
};

} // namespace datalogger

