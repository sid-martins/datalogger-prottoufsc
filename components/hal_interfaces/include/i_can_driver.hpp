#pragma once

#include <array>
#include <cstdint>
#include "board_pins.h"

namespace datalogger::hal {

/**
 * @brief Velocidades (Baudrates) suportadas pelo barramento CAN (TWAI).
 */
enum class CanBaudRate : uint32_t {
    k250Kbps = 250000,
    k500Kbps = 500000,
    k1Mbps   = 1000000
};

/**
 * @brief Modos de operação do controlador CAN / transceptor SN65HVD230DR.
 */
enum class CanOperatingMode {
    kNormal,      // Operação normal (envia e recebe com ACK)
    kListenOnly,  // Modo passivo/sniffer (apenas escuta sem enviar ACK no barramento)
    kNoAck        // Modo loopback/autoteste sem exigir ACK externo
};

/**
 * @brief Estado atual do controlador CAN (TWAI).
 */
enum class CanBusState {
    kStopped,
    kRunning,
    kBusOff,
    kRecovering
};

/**
 * @brief Configuração de inicialização do barramento CAN (mapeado por padrão nos pinos do esquemático).
 */
struct CanConfig {
    gpio_num_t tx_pin{PIN_CAN_TX};                       // GPIO 15 (CAN_TX -> Pino D do SN65HVD230DR)
    gpio_num_t rx_pin{PIN_CAN_RX};                       // GPIO 16 (CAN_RX <- Pino R do SN65HVD230DR)
    CanBaudRate baud_rate{CanBaudRate::k500Kbps};        // Padrão 500 kbps (Automotivo / ECUs / OBD2)
    CanOperatingMode mode{CanOperatingMode::kNormal};
    uint32_t tx_queue_len{16};
    uint32_t rx_queue_len{64};
};

/**
 * @brief Representa um frame CAN 2.0A (Standard 11-bit) ou CAN 2.0B (Extended 29-bit).
 */
struct CanFrame {
    static constexpr uint32_t kMaxStandardId = 0x7FFu;
    static constexpr uint32_t kMaxExtendedId = 0x1FFFFFFFu;
    static constexpr uint8_t kMaxDlc = 8u;

    uint32_t id{0};                          // Identificador CAN (11 bits ou 29 bits)
    bool is_extended{false};                 // false = Standard (11-bit), true = Extended (29-bit)
    bool is_rtr{false};                      // Remote Transmission Request
    uint8_t dlc{0};                          // Data Length Code (0 a 8 bytes)
    std::array<uint8_t, kMaxDlc> data{};     // Payload de até 8 bytes
    uint64_t timestamp_ms{0};                // Timestamp de recepção/transmissão em ms
};

/**
 * @brief Interface de Abstração de Hardware (HAL) para o controlador CAN (ESP32-S3 TWAI + SN65HVD230DR).
 */
class ICanDriver {
public:
    virtual ~ICanDriver() = default;

    /**
     * @brief Instala o driver TWAI e configura os pinos TX/RX e timing (baudrate).
     */
    virtual bool init(const CanConfig& config) = 0;

    /**
     * @brief Inicia o controlador CAN no barramento.
     */
    virtual bool start() = 0;

    /**
     * @brief Para o controlador CAN.
     */
    virtual bool stop() = 0;

    /**
     * @brief Transmite um frame CAN no barramento.
     * @param frame Frame a ser transmitido.
     * @param timeout_ms Tempo máximo de espera na fila de TX.
     */
    virtual bool transmit(const CanFrame& frame, uint32_t timeout_ms) = 0;

    /**
     * @brief Recebe um frame CAN do barramento.
     * @param[out] out_frame Estrutura que receberá o frame lido.
     * @param timeout_ms Tempo máximo de espera na fila de RX.
     */
    virtual bool receive(CanFrame& out_frame, uint32_t timeout_ms) = 0;

    /**
     * @brief Consulta o estado atual do controlador CAN (Running, Bus-Off, etc.).
     */
    virtual CanBusState get_state() const = 0;

    /**
     * @brief Inicia o processo de recuperação caso o controlador entre em estado Bus-Off.
     */
    virtual bool initiate_recovery() = 0;
};

} // namespace datalogger::hal

