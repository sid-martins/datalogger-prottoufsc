#pragma once

#include <cstdint>
#include "i_can_driver.hpp"
#include "i_rtos_queue.hpp"

namespace datalogger::can {

/**
 * @brief Controlador de alto nível do barramento CAN (transceptor SN65HVD230DR).
 *
 * Responsável por:
 * - Inicializar o barramento com a configuração padrão do esquemático (GPIO 15 TX / GPIO 16 RX @ 500 kbps);
 * - Validar identificadores Standard (11-bit) e Extended (29-bit) e tamanho de payload (DLC <= 8);
 * - Enfileirar frames recebidos em uma fila do FreeRTOS (IRtosQueue<CanFrame>) para gravação/telemetria;
 * - Monitorar e recuperar automaticamente o controlador caso entre em estado Bus-Off.
 */
class CanController {
public:
    explicit CanController(hal::ICanDriver& driver);

    /**
     * @brief Inicializa e dá partida no controlador CAN.
     * @param config Configuração de pinos, velocidade e filas (padrão: GPIO 15/16 @ 500 kbps).
     */
    bool initialize(const hal::CanConfig& config = hal::CanConfig{});

    /**
     * @brief Valida e transmite um frame CAN 2.0A (11-bit) ou CAN 2.0B (29-bit).
     * @param frame Frame a ser enviado.
     * @param timeout_ms Timeout de envio em milissegundos.
     */
    bool send_frame(const hal::CanFrame& frame, uint32_t timeout_ms = 50);

    /**
     * @brief Passo executado pela Task de Recepção CAN do FreeRTOS:
     * Aguarda um frame vindo do transceptor SN65HVD230DR e o envia para a fila de recepção.
     * @param rx_queue Fila do FreeRTOS que receberá o frame.
     * @param rx_timeout_ms Timeout aguardando frame no barramento CAN.
     * @param queue_timeout_ms Timeout para inserir na fila do FreeRTOS.
     */
    bool receive_to_queue(hal::IRtosQueue<hal::CanFrame>& rx_queue,
                          uint32_t rx_timeout_ms = 100,
                          uint32_t queue_timeout_ms = 10);

    /**
     * @brief Verifica se o controlador entrou em Bus-Off e dispara a recuperação automática.
     * @return true se uma recuperação foi iniciada com sucesso, false caso contrário.
     */
    bool check_and_recover_bus();

    /**
     * @brief Valida se um frame CAN respeita os limites de ID (11-bit ou 29-bit) e DLC (0..8).
     */
    static bool is_frame_valid(const hal::CanFrame& frame);

    bool is_initialized() const { return initialized_; }
    uint32_t tx_count() const { return tx_count_; }
    uint32_t rx_count() const { return rx_count_; }
    uint32_t tx_error_count() const { return tx_error_count_; }
    uint32_t dropped_rx_count() const { return dropped_rx_count_; }
    uint32_t bus_off_recovery_count() const { return bus_off_recovery_count_; }

private:
    hal::ICanDriver& driver_;
    bool initialized_{false};
    uint32_t tx_count_{0};
    uint32_t rx_count_{0};
    uint32_t tx_error_count_{0};
    uint32_t dropped_rx_count_{0};
    uint32_t bus_off_recovery_count_{0};
};

} // namespace datalogger::can

