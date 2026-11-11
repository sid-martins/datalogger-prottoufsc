#pragma once

#include <cstdint>

namespace datalogger::hal {

/**
 * @brief Estrutura representando uma leitura bruta dos sensores do datalogger.
 */
struct SensorReading {
    uint64_t timestamp_ms{0};
    float temperature_c{0.0f};
    float humidity_pct{0.0f};
    float battery_mv{0.0f};
};

/**
 * @brief Interface de Abstração de Hardware (HAL) para sensores.
 * Permite injetar um Mock (GMock) nos testes TDD ou o driver real (I2C/SPI/ADC) no ESP32-S3.
 */
class ISensor {
public:
    virtual ~ISensor() = default;

    /**
     * @brief Inicializa o sensor.
     * @return true se inicializado com sucesso, false caso contrário.
     */
    virtual bool init() = 0;

    /**
     * @brief Realiza a leitura de uma amostra do sensor.
     * @param[out] out_reading Estrutura onde os dados lidos serão armazenados.
     * @return true se a leitura foi bem-sucedida, false em caso de erro de comunicação/CRC.
     */
    virtual bool read(SensorReading& out_reading) = 0;
};

} // namespace datalogger::hal

