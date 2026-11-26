#pragma once

#include <cstdint>

namespace datalogger::hal {

/**
 * @brief Resultado da leitura de uma chave na memória não-volátil (NVS).
 */
enum class NvmReadResult : uint8_t {
    kOk       = 0, // Chave encontrada e lida com sucesso
    kNotFound = 1, // Chave ainda não gravada (ex.: primeiro boot de fábrica)
    kError    = 2  // Falha de hardware/partição ou argumento inválido
};

/**
 * @brief Configuração da partição e namespace NVS utilizados pelo serviço.
 */
struct NvmConfig {
    const char* partition_name{"nvs"};
    const char* namespace_name{"datalogger"};
};

/**
 * @brief HAL (Hardware Abstraction Layer) para Memória Não-Volátil (NVS / EEPROM emulada em Flash).
 *
 * Isola as chamadas nativas `nvs_flash` / `nvs_handle_t` do ESP-IDF para permitir
 * testes determinísticos Software-in-the-Loop (SIL) de contadores permanentes como o
 * Horímetro de Motor (*Engine Run Time Total*).
 */
class INvmHal {
public:
    virtual ~INvmHal() = default;

    /**
     * @brief Inicializa a partição não-volátil e abre o namespace em modo leitura/escrita.
     */
    virtual bool init(const NvmConfig& config) = 0;

    /**
     * @brief Lê um inteiro sem sinal de 32 bits associado à chave (`<= 15` caracteres).
     */
    virtual NvmReadResult read_u32(const char* key, uint32_t& out_value) const = 0;

    /**
     * @brief Agenda a gravação de um inteiro de 32 bits associado à chave (`<= 15` caracteres).
     */
    virtual bool write_u32(const char* key, uint32_t value) = 0;

    /**
     * @brief Efetiva fisicamente na memória não-volátil todas as escritas pendentes.
     */
    virtual bool commit() = 0;
};

} // namespace datalogger::hal
