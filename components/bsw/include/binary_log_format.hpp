#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace datalogger::bsw::binlog {

/**
 * @brief Identificador mágico de arquivo do protocolo binário PDLG v1 ("PDLG").
 */
inline constexpr std::array<char, 4> kFileMagic{'P', 'D', 'L', 'G'};

inline constexpr uint16_t kFormatVersion = 1u;
inline constexpr std::size_t kFileHeaderSize = 256u;
inline constexpr std::size_t kRecordSize = 256u;

/**
 * @brief Palavra de sincronização no início de cada registro de 256 bytes (wire LE: 0x5A, 0xA5).
 */
inline constexpr uint16_t kRecordSyncWord = 0xA55Au;
inline constexpr uint8_t kRecordTypeTelemetry = 0x01u;

inline constexpr uint16_t kHeaderFlagRtcSynced = 0x0001u;
inline constexpr uint8_t kRecordFlagRtcSynced = 0x01u;
inline constexpr uint8_t kRecordFlagGpsHasFix = 0x02u;

inline constexpr std::size_t kAdcTagSlotSize = 16u;
inline constexpr std::size_t kDeviceIdSlotSize = 32u;
inline constexpr std::string_view kDeviceId{"PROTTO-UFSC-ESP32S3-V1"};
inline constexpr std::string_view kFileExtension{".bin"};

/**
 * @brief Polinômio refletido IEEE 802.3 do CRC-32 (init=0xFFFFFFFF, xorout=0xFFFFFFFF).
 */
inline constexpr uint32_t kCrc32Poly = 0xEDB88320u;

/**
 * @brief Metadados da sessão gravados uma única vez no cabeçalho binário (256 bytes).
 */
struct BinarySessionHeaderMeta {
    uint64_t start_monotonic_ms{0};
    uint64_t start_epoch_ms{0};
    uint16_t nominal_rate_hz{100u};
    bool rtc_synchronized{false};
};

} // namespace datalogger::bsw::binlog
