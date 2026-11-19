#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include "binary_log_format.hpp"
#include "i_osal_queue.hpp"
#include "i_storage_hal.hpp"
#include "rtc_service.hpp"
#include "telemetry_snapshot.hpp"

namespace datalogger::bsw {

/**
 * @brief BSW (Basic Software Service) - Serviço de gravação binária em MicroSD (PDLG v1).
 *
 * Consome `TelemetrySnapshot` da fila OSAL no Core 0, serializa registros binários fixos
 * de 256 bytes (Little-Endian com Sync Word `0xA55A` e CRC-32 IEEE 802.3), acumula em um
 * bloco estático de 4 KB para reduzir desgaste e latência no cartão SD, e executa `sync()`
 * periódico para preservar integridade em caso de corte brusco de energia.
 */
class LoggerService {
public:
    static constexpr std::size_t kMaxPathLen = 96u;
    static constexpr std::size_t kFileHeaderSize = binlog::kFileHeaderSize;
    static constexpr std::size_t kRecordSize = binlog::kRecordSize;
    static constexpr std::size_t kWriteBlockBufferSize = 4096u;
    static constexpr uint32_t kDefaultSyncEveryNSamples = 50u;
    static constexpr uint16_t kDefaultNominalRateHz = 100u;
    static constexpr uint64_t kMinFreeSpaceBytes = 1024ULL * 1024ULL;

    static_assert((kWriteBlockBufferSize % kRecordSize) == 0u,
                  "Buffer de bloco deve ser multiplo exato do tamanho do registro binario");

    explicit LoggerService(hal::IStorageHal& storage_hal,
                           uint32_t sync_every_n_samples = kDefaultSyncEveryNSamples,
                           uint16_t nominal_rate_hz = kDefaultNominalRateHz);

    /**
     * @brief Salva a configuração de armazenamento e monta a mídia via `IStorageHal`.
     */
    bool initialize(const hal::StorageConfig& config = hal::StorageConfig{});

    /**
     * @brief Abre nova sessão (`LOG_<session_ts>.bin`), grava cabeçalho PDLG de 256 bytes e sincroniza.
     */
    bool start_session(const RtcService& rtc_service);

    /**
     * @brief Serializa um snapshot em registro binário de 256 bytes e grava no bloco / MicroSD.
     */
    bool log_snapshot(const TelemetrySnapshot& snapshot);

    /**
     * @brief Retira um snapshot da fila OSAL e o registra no arquivo de sessão ativo.
     */
    bool process_queue_step(osal::IOsalQueue<TelemetrySnapshot>& queue,
                            uint32_t receive_timeout_ms = 100u);

    /**
     * @brief Descarrega o bloco pendente para o HAL e executa `sync()`.
     */
    bool flush();

    /**
     * @brief Finaliza a sessão ativa garantindo flush, sync e fechamento seguro do arquivo.
     */
    bool stop_session();

    /**
     * @brief Calcula o checksum CRC-32 IEEE 802.3 (`poly=0xEDB88320`, `init=0xFFFFFFFF`, `xorout=0xFFFFFFFF`).
     */
    static uint32_t compute_crc32(const uint8_t* data, std::size_t len);

    /**
     * @brief Serializa o cabeçalho binário autodescritivo de 256 bytes (`PDLG v1`) em Little-Endian.
     */
    static std::size_t encode_file_header(const binlog::BinarySessionHeaderMeta& meta,
                                          uint8_t* out_buf,
                                          std::size_t buf_size);

    /**
     * @brief Serializa os campos de `TelemetrySnapshot` em registro binário fixo de 256 bytes (Little-Endian + CRC-32).
     */
    static std::size_t encode_telemetry_record(const TelemetrySnapshot& snapshot,
                                               uint8_t* out_buf,
                                               std::size_t buf_size);

    bool is_mounted() const { return storage_hal_.is_mounted(); }
    bool is_session_active() const { return session_active_; }
    std::string_view current_file_path() const { return std::string_view(current_file_path_.data()); }
    uint32_t samples_logged() const { return samples_logged_; }
    uint64_t bytes_written() const { return bytes_written_; }
    uint32_t sync_count() const { return sync_count_; }
    uint32_t storage_error_count() const { return storage_error_count_; }
    std::size_t pending_block_bytes() const { return block_len_; }

private:
    bool build_session_file_path(std::string_view session_ts);
    bool flush_block();

    hal::IStorageHal& storage_hal_;
    uint32_t sync_every_n_samples_;
    uint16_t nominal_rate_hz_;
    hal::StorageConfig config_{};
    std::array<char, 32> mount_point_buf_{};
    std::array<char, kMaxPathLen> current_file_path_{};
    std::array<uint8_t, kWriteBlockBufferSize> block_buf_{};
    std::size_t block_len_{0};
    bool session_active_{false};
    uint32_t samples_logged_{0};
    uint32_t samples_since_sync_{0};
    uint64_t bytes_written_{0};
    uint32_t sync_count_{0};
    uint32_t storage_error_count_{0};
};

} // namespace datalogger::bsw
