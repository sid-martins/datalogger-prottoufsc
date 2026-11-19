#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace datalogger::bsw {

/**
 * @brief Identificadores CAN e constantes do protocolo distribuído Protto UFSC + FuelTech FTCAN 2.0.
 *
 * - Nós Satélites ESP32-C3: Standard ID (11-bit, is_extended = false), Little-Endian, DLC = 8 bytes.
 * - ECU FuelTech (FT450/FT550/FT600): Extended ID (29-bit, ex: 0x14080600..0x14080602) ou
 *   Standard ID (11-bit, 0x600..0x602), Big-Endian (Motorola), DLC = 8 bytes.
 */
namespace can_id {

// 1. Nó de Freio e Direção (ESP32-C3) - Alta Prioridade
inline constexpr uint32_t kNodeFrontControlsId = 0x100u;

// 2. Nós das 4 Rodas (ESP32-C3) - FL, FR, RL, RR
inline constexpr uint32_t kNodeWheelFlId       = 0x200u;
inline constexpr uint32_t kNodeWheelFrId       = 0x201u;
inline constexpr uint32_t kNodeWheelRlId       = 0x202u;
inline constexpr uint32_t kNodeWheelRrId       = 0x203u;

// 3. Nó de IMU WIT (ESP32-C3) - Acelerômetro e Giroscópio Calibrados
inline constexpr uint32_t kNodeImuAccelId      = 0x300u;
inline constexpr uint32_t kNodeImuGyroId       = 0x301u;

// 4. ECU FuelTech (FTCAN 2.0 Simplified Broadcast - MessageIDs 0x600..0x602)
inline constexpr uint32_t kEcuFuelTechMsg1Id    = 0x600u;
inline constexpr uint32_t kEcuFuelTechMsg2Id    = 0x601u;
inline constexpr uint32_t kEcuFuelTechMsg3Id    = 0x602u;

inline constexpr uint32_t kEcuFuelTechExtMsg1Id = 0x14080600u;
inline constexpr uint32_t kEcuFuelTechExtMsg2Id = 0x14080601u;
inline constexpr uint32_t kEcuFuelTechExtMsg3Id = 0x14080602u;

inline constexpr uint32_t kFuelTechMessageIdMask = 0x7FFu;

} // namespace can_id

/**
 * @brief Enumeração dos nós físicos conectados ao barramento CAN do veículo.
 */
enum class CanNodeId : uint8_t {
    kFrontControls = 0, // Nó ESP32-C3 Freio/Direção (0x100)
    kWheelFl       = 1, // Nó ESP32-C3 Roda Dianteira Esquerda (0x200)
    kWheelFr       = 2, // Nó ESP32-C3 Roda Dianteira Direita (0x201)
    kWheelRl       = 3, // Nó ESP32-C3 Roda Traseira Esquerda (0x202)
    kWheelRr       = 4, // Nó ESP32-C3 Roda Traseira Direita (0x203)
    kImuWit        = 5, // Nó ESP32-C3 IMU WIT (0x300 / 0x301)
    kEcuFuelTech   = 6, // ECU FuelTech (0x600..0x602 / 0x14080600..0x14080602)
    kCount         = 7
};

/**
 * @brief Índices das 4 rodas dentro do array `VehicleCanState::wheels`.
 */
enum class WheelCorner : uint8_t {
    kFrontLeft  = 0,
    kFrontRight = 1,
    kRearLeft   = 2,
    kRearRight  = 3,
    kCount      = 4
};

/**
 * @brief Dados decodificados do Nó de Freio e Direção (ID 0x100).
 */
struct FrontControlsData {
    float steering_angle_deg{0.0f};       // Bytes 0..1: int16_t  (0.1 deg/bit)
    float brake_pressure_front_bar{0.0f}; // Bytes 2..3: uint16_t (0.1 bar/bit)
    float brake_pressure_rear_bar{0.0f};  // Bytes 4..5: uint16_t (0.1 bar/bit)
    float brake_pedal_pos_pct{0.0f};      // Bytes 6..7: uint16_t (0.1 %/bit)
};

/**
 * @brief Dados decodificados de um Nó de Roda (IDs 0x200..0x203).
 */
struct WheelNodeData {
    float damper_pos_mm{0.0f};     // Bytes 0..1: uint16_t (0.1 mm/bit) - Potenciômetro Linear
    float wheel_speed_kmh{0.0f};   // Bytes 2..3: uint16_t (0.1 km/h/bit) - Velocidade da Roda
    float load_cell_n{0.0f};       // Bytes 4..5: int16_t  (1.0 N/bit) - Célula de Carga
    float brake_disc_temp_c{0.0f}; // Bytes 6..7: int16_t  (0.1 C/bit) - Temp. Disco de Freio
};

/**
 * @brief Dados decodificados do Nó de IMU WIT (IDs 0x300 e 0x301).
 */
struct ImuWitData {
    float accel_x_mps2{0.0f};   // 0x300 Bytes 0..1: int16_t (0.01 m/s^2/bit)
    float accel_y_mps2{0.0f};   // 0x300 Bytes 2..3: int16_t (0.01 m/s^2/bit)
    float accel_z_mps2{0.0f};   // 0x300 Bytes 4..5: int16_t (0.01 m/s^2/bit)
    uint16_t status_flags{0};   // 0x300 Bytes 6..7: uint16_t
    float gyro_x_dps{0.0f};     // 0x301 Bytes 0..1: int16_t (0.1 deg/s/bit)
    float gyro_y_dps{0.0f};     // 0x301 Bytes 2..3: int16_t (0.1 deg/s/bit)
    float gyro_z_dps{0.0f};     // 0x301 Bytes 4..5: int16_t (0.1 deg/s/bit)
};

/**
 * @brief Dados decodificados da ECU FuelTech (FTCAN 2.0 Simplified Broadcast 0x600..0x602).
 */
struct EcuFuelTechData {
    // Bloco 0x600 (0x14080600)
    float tps_pct{0.0f};             // Bytes 0..1: int16_t BE (0.1 %/bit)
    float map_bar{0.0f};             // Bytes 2..3: int16_t BE (0.001 bar/bit)
    float air_temp_c{0.0f};          // Bytes 4..5: int16_t BE (0.1 C/bit)
    float engine_temp_c{0.0f};       // Bytes 6..7: int16_t BE (0.1 C/bit)

    // Bloco 0x601 (0x14080601)
    float oil_pressure_bar{0.0f};    // Bytes 0..1: int16_t BE (0.001 bar/bit)
    float fuel_pressure_bar{0.0f};   // Bytes 2..3: int16_t BE (0.001 bar/bit)
    float water_pressure_bar{0.0f};  // Bytes 4..5: int16_t BE (0.001 bar/bit)
    int16_t gear{0};                 // Bytes 6..7: int16_t BE (1/bit: -1=R, 0=N, 1..6=Marcha)

    // Bloco 0x602 (0x14080602)
    float exhaust_o2_lambda{0.0f};   // Bytes 0..1: int16_t BE (0.001 lambda/bit)
    uint16_t rpm{0};                 // Bytes 2..3: uint16_t BE (1 rpm/bit)
    float oil_temp_c{0.0f};          // Bytes 4..5: int16_t BE (0.1 C/bit)
    uint16_t pit_limit_status{0};    // Bytes 6..7: uint16_t BE
};

/**
 * @brief Tabela de Estado Consolidada da Rede CAN do Veículo (Blackboard estático sem heap).
 */
struct VehicleCanState {
    static constexpr std::size_t kNodeCount = static_cast<std::size_t>(CanNodeId::kCount);
    static constexpr std::size_t kWheelCount = static_cast<std::size_t>(WheelCorner::kCount);

    FrontControlsData front_controls{};
    std::array<WheelNodeData, kWheelCount> wheels{};
    ImuWitData imu{};
    EcuFuelTechData ecu{};

    std::array<uint64_t, kNodeCount> last_rx_timestamp_ms{};
    std::array<bool, kNodeCount> node_ever_received{};
};

} // namespace datalogger::bsw
