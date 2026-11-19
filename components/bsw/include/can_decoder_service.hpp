#pragma once

#include <cstdint>
#include "can_protocol.hpp"
#include "i_can_hal.hpp"

namespace datalogger::bsw {

/**
 * @brief BSW (Basic Software Service) - Decodificador e Gerenciador de Estado da Rede CAN Distribuída.
 *
 * Decodifica pacotes dos 6 nós ESP32-C3 (4x Rodas, 1x Freio/Direção, 1x IMU WIT) em Little-Endian
 * e os pacotes de Broadcast da ECU FuelTech (FTCAN 2.0 29-bit ou 11-bit) em Big-Endian,
 * mantendo a tabela estática `VehicleCanState` e monitorando timeouts de desconexão por nó.
 */
class CanDecoderService {
public:
    static constexpr uint64_t kDefaultNodeTimeoutMs = 200u;
    static constexpr uint8_t kRequiredFrameDlc = 8u;

    CanDecoderService() = default;

    /**
     * @brief Processa e decodifica um frame CAN recebido, atualizando a tabela `VehicleCanState`.
     * @param frame Frame CAN bruto vindo do `CanService` / `ICanHal`.
     * @return true se o frame pertence ao protocolo e foi decodificado com sucesso.
     */
    bool decode_frame(const hal::CanFrame& frame);

    /**
     * @brief Verifica se um nó específico da rede CAN está ativo (dentro da janela de timeout).
     * @param node Identificador do nó (`CanNodeId`).
     * @param current_time_ms Timestamp atual do sistema em milissegundos.
     * @param timeout_ms Tempo máximo tolerado sem novas mensagens do nó.
     * @return true se o nó já enviou ao menos 1 pacote e a idade do último pacote é <= timeout_ms.
     */
    bool is_node_online(CanNodeId node,
                        uint64_t current_time_ms,
                        uint64_t timeout_ms = kDefaultNodeTimeoutMs) const;

    /**
     * @brief Reinicia todos os dados decodificados e contadores de diagnóstico.
     */
    void reset();

    const VehicleCanState& state() const { return state_; }
    uint32_t decoded_frame_count() const { return decoded_frame_count_; }
    uint32_t unknown_id_count() const { return unknown_id_count_; }
    uint32_t decode_error_count() const { return decode_error_count_; }

private:
    static uint16_t unpack_u16_le(const uint8_t* bytes);
    static int16_t unpack_i16_le(const uint8_t* bytes);
    static uint16_t unpack_u16_be(const uint8_t* bytes);
    static int16_t unpack_i16_be(const uint8_t* bytes);

    void mark_node_received(CanNodeId node, uint64_t timestamp_ms);
    void decode_front_controls(const hal::CanFrame& frame);
    void decode_wheel_node(WheelCorner corner, CanNodeId node, const hal::CanFrame& frame);
    void decode_imu_accel(const hal::CanFrame& frame);
    void decode_imu_gyro(const hal::CanFrame& frame);
    void decode_fueltech_msg1(const hal::CanFrame& frame);
    void decode_fueltech_msg2(const hal::CanFrame& frame);
    void decode_fueltech_msg3(const hal::CanFrame& frame);

    VehicleCanState state_{};
    uint32_t decoded_frame_count_{0};
    uint32_t unknown_id_count_{0};
    uint32_t decode_error_count_{0};
};

} // namespace datalogger::bsw
