#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "driver/uart.h"
#include "i_gps_hal.hpp"

namespace datalogger::lld {

/**
 * @brief LLD (Low-Level Driver / MCAL) do módulo GPS u-blox NEO-8M / NEO-M8 via UART (ESP-IDF).
 *
 * Implementa `hal::IGpsHal` usando `driver/uart.h` nos pinos `PIN_GPS_RX` / `PIN_GPS_TX`.
 * Na inicialização, configura a UART em 9600 baud (padrão de fábrica), envia a sequência
 * binária UBX (`UBX-CFG-PRT` -> 115200 baud, `UBX-CFG-MSG` -> apenas GGA/RMC,
 * `UBX-CFG-RATE` -> 10 Hz / 100 ms, `UBX-CFG-NAV5` -> Automotive) e reconfigura a UART
 * do ESP32-S3 para o baud operacional com tentativa adicional para convergência em warm-start.
 */
class Neo8mUartLld : public hal::IGpsHal {
public:
    static constexpr uint8_t kUbxSync1        = 0xB5u;
    static constexpr uint8_t kUbxSync2        = 0x62u;
    static constexpr uint8_t kUbxClassCfg     = 0x06u;
    static constexpr uint8_t kUbxIdCfgPrt     = 0x00u;
    static constexpr uint8_t kUbxIdCfgMsg     = 0x01u;
    static constexpr uint8_t kUbxIdCfgRate    = 0x08u;
    static constexpr uint8_t kUbxIdCfgNav5    = 0x24u;

    static constexpr uint8_t kNmeaClass       = 0xF0u;
    static constexpr uint8_t kNmeaIdGga       = 0x00u;
    static constexpr uint8_t kNmeaIdGll       = 0x01u;
    static constexpr uint8_t kNmeaIdGsa       = 0x02u;
    static constexpr uint8_t kNmeaIdGsv       = 0x03u;
    static constexpr uint8_t kNmeaIdRmc       = 0x04u;
    static constexpr uint8_t kNmeaIdVtg       = 0x05u;

    static constexpr std::size_t kUbxHeaderLen      = 6u;
    static constexpr std::size_t kUbxChecksumLen    = 2u;
    static constexpr std::size_t kCfgPrtPayloadLen  = 20u;
    static constexpr std::size_t kCfgMsgPayloadLen  = 3u;
    static constexpr std::size_t kCfgRatePayloadLen = 6u;
    static constexpr std::size_t kCfgNav5PayloadLen = 36u;

    static constexpr std::size_t kCfgPrtFrameLen  = kUbxHeaderLen + kCfgPrtPayloadLen + kUbxChecksumLen;
    static constexpr std::size_t kCfgMsgFrameLen  = kUbxHeaderLen + kCfgMsgPayloadLen + kUbxChecksumLen;
    static constexpr std::size_t kCfgRateFrameLen = kUbxHeaderLen + kCfgRatePayloadLen + kUbxChecksumLen;
    static constexpr std::size_t kCfgNav5FrameLen = kUbxHeaderLen + kCfgNav5PayloadLen + kUbxChecksumLen;

    static constexpr uint8_t kUartPortId          = 1u;
    static constexpr uint32_t kUartMode8N1        = 0x000008D0u;
    static constexpr uint16_t kProtoMaskUbxNmea   = 0x0003u;
    static constexpr uint16_t kProtoMaskNmeaOnly  = 0x0002u;
    static constexpr uint16_t kNav5MaskDyn        = 0x0001u;
    static constexpr uint16_t kNav5MaskDynAndFix  = 0x0005u;
    static constexpr uint8_t kNav5FixModeAuto     = 3u;

    static constexpr int kUartRxBufferSize        = 2048;
    static constexpr int kUartTxBufferSize        = 0;
    static constexpr uint32_t kWarmupSettleMs     = 100u;
    static constexpr uint32_t kBaudSwitchSettleMs = 20u;
    static constexpr uint32_t kTxDoneTimeoutMs    = 100u;
    static constexpr uint8_t kMaxUpdateRateHz     = 10u;
    static constexpr uint8_t kMaxDynamicModel     = 10u;

    Neo8mUartLld() = default;
    ~Neo8mUartLld() override;

    Neo8mUartLld(const Neo8mUartLld&) = delete;
    Neo8mUartLld& operator=(const Neo8mUartLld&) = delete;

    /**
     * @brief Valida a configuração, instala o driver UART e envia a sequência UBX de 10 Hz.
     */
    bool init(const hal::GpsConfig& config) override;

    /**
     * @brief Lê até `max_len` bytes do buffer UART respeitando `timeout_ms`.
     */
    int read_bytes(uint8_t* buffer, std::size_t max_len, uint32_t timeout_ms) override;

    /**
     * @brief Transmite `len` bytes pela UART se o driver estiver inicializado.
     */
    bool write_bytes(const uint8_t* data, std::size_t len) override;

    bool is_initialized() const { return initialized_; }

    /**
     * @brief Calcula o checksum Fletcher-8 (CK_A, CK_B) do protocolo UBX sobre `data[0..len-1]`.
     */
    static void compute_ubx_checksum(const uint8_t* data,
                                     std::size_t len,
                                     uint8_t& out_ck_a,
                                     uint8_t& out_ck_b);

    /**
     * @brief Monta o comando binário `UBX-CFG-PRT` (0x06 0x00) para a UART1 em formato 8N1.
     */
    static std::array<uint8_t, kCfgPrtFrameLen> build_cfg_prt(
        uint32_t baud_rate,
        uint16_t in_proto_mask = kProtoMaskUbxNmea,
        uint16_t out_proto_mask = kProtoMaskUbxNmea);

    /**
     * @brief Monta o comando binário `UBX-CFG-MSG` (0x06 0x01, variante 3 bytes na porta atual).
     */
    static std::array<uint8_t, kCfgMsgFrameLen> build_cfg_msg(
        uint8_t msg_class,
        uint8_t msg_id,
        uint8_t rate);

    /**
     * @brief Monta o comando binário `UBX-CFG-RATE` (0x06 0x08) para taxa de medição/navegação.
     */
    static std::array<uint8_t, kCfgRateFrameLen> build_cfg_rate(
        uint16_t meas_rate_ms,
        uint16_t nav_rate = 1u,
        uint16_t time_ref = 0u);

    /**
     * @brief Monta o comando binário `UBX-CFG-NAV5` (0x06 0x24) para modelo dinâmico e fix mode.
     */
    static std::array<uint8_t, kCfgNav5FrameLen> build_cfg_nav5(
        uint8_t dyn_model,
        uint16_t mask = kNav5MaskDyn,
        uint8_t fix_mode = 0u);

private:
    void deinit();
    bool send_ubx_frame(const uint8_t* frame, std::size_t len);

    uart_port_t uart_port_{UART_NUM_1};
    hal::GpsConfig config_{};
    bool driver_installed_{false};
    bool initialized_{false};
};

} // namespace datalogger::lld
