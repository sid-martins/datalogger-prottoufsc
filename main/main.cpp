#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "adc_oneshot_lld.hpp"
#include "adc_service.hpp"
#include "bsp_pins.h"
#include "can_decoder_service.hpp"
#include "can_service.hpp"
#include "can_twai_lld.hpp"
#include "ds3231_i2c_lld.hpp"
#include "engine_hours_service.hpp"
#include "freertos_queue_adapter.hpp"
#include "gpio_lld.hpp"
#include "gps_service.hpp"
#include "logger_service.hpp"
#include "neo8m_uart_lld.hpp"
#include "nvs_lld.hpp"
#include "rtc_service.hpp"
#include "sd_card_lld.hpp"
#include "telemetry_aggregator.hpp"
#include "telemetry_snapshot.hpp"
#include "ui_service.hpp"

namespace {

constexpr const char* kTag = "ASW_MAIN";
constexpr TickType_t kAcquisitionPeriodTicks = pdMS_TO_TICKS(10); // 100 Hz (Core 1)
constexpr TickType_t kUiPeriodTicks          = pdMS_TO_TICKS(20); // 50 Hz  (Core 0)
constexpr TickType_t kCanRecoveryPollTicks   = pdMS_TO_TICKS(10);
constexpr uint64_t kStorageRetryCooldownMs   = 2000ULL;

/**
 * @brief Estado compartilhado lock-free entre as tasks Dual-Core (Core 1 <-> Core 0).
 */
struct SharedRuntimeState {
    std::atomic<uint16_t> latest_ecu_rpm{0u};
    std::atomic<bool> ecu_online{false};
    std::atomic<bool> can_warning{false};
    std::atomic<uint16_t> lap_beacon{0u};
    std::atomic<uint32_t> engine_hours_total_s{0u};
    std::atomic<bool> recording_requested{false};
    std::atomic<bool> session_active{false};
    std::atomic<bool> storage_fault{false};
};

struct CanRxTaskContext {
    datalogger::bsw::CanService* service;
    datalogger::osal::IOsalQueue<datalogger::hal::CanFrame>* rx_queue;
};

struct AcquisitionTaskContext {
    datalogger::osal::IOsalQueue<datalogger::hal::CanFrame>* can_rx_queue;
    datalogger::bsw::CanDecoderService* can_decoder;
    datalogger::bsw::AdcService* adc_service;
    datalogger::bsw::GpsService* gps_service;
    const datalogger::bsw::RtcService* rtc_service;
    datalogger::bsw::TelemetryAggregator* aggregator;
    datalogger::osal::IOsalQueue<datalogger::bsw::TelemetrySnapshot>* telemetry_queue;
    SharedRuntimeState* shared;
};

struct UiTaskContext {
    datalogger::bsw::UiService* ui_service;
    datalogger::bsw::EngineHoursService* engine_hours_service;
    const datalogger::bsw::RtcService* rtc_service;
    SharedRuntimeState* shared;
};

struct StorageTaskContext {
    datalogger::bsw::LoggerService* logger_service;
    const datalogger::bsw::RtcService* rtc_service;
    datalogger::osal::IOsalQueue<datalogger::bsw::TelemetrySnapshot>* telemetry_queue;
    SharedRuntimeState* shared;
};

/**
 * @brief ASW Task de Recepção CAN (FreeRTOS Core 1 - APP_CPU)
 * Lê frames do transceptor SN65HVD230DR e os enfileira sem bloqueio.
 */
void can_rx_task(void* pvParameters) {
    auto* ctx = static_cast<CanRxTaskContext*>(pvParameters);

    while (true) {
        ctx->service->check_and_recover_bus();
        if (ctx->service->bus_state() != datalogger::hal::CanBusState::kRunning) {
            vTaskDelay(kCanRecoveryPollTicks);
            continue;
        }
        if (!ctx->service->receive_to_queue(*ctx->rx_queue, 100, 10) &&
            ctx->service->bus_state() != datalogger::hal::CanBusState::kRunning) {
            vTaskDelay(kCanRecoveryPollTicks);
        }
    }
}

/**
 * @brief ASW Task Determinística de Aquisição 100 Hz (FreeRTOS Core 1 - APP_CPU)
 * Drena frames CAN para o decodificador, amostra ADC1 (multi-taxa), lê GPS, atualiza
 * telemetria de RPM/diagnóstico CAN e publica snapshots para o Core 0 durante gravação.
 */
void acquisition_task(void* pvParameters) {
    auto* ctx = static_cast<AcquisitionTaskContext*>(pvParameters);
    TickType_t last_wake_time = xTaskGetTickCount();
    datalogger::hal::CanFrame frame{};

    while (true) {
        for (int i = 0; i < 64 && ctx->can_rx_queue->receive(frame, 0); ++i) {
            ctx->can_decoder->decode_frame(frame);
        }

        const uint64_t now_ms = ctx->rtc_service->sample_timestamp().monotonic_ms;
        ctx->adc_service->sample_tick(now_ms);
        ctx->gps_service->poll(0);

        const auto& can_state = ctx->can_decoder->state();
        const bool ecu_on = ctx->can_decoder->is_node_online(
            datalogger::bsw::CanNodeId::kEcuFuelTech, now_ms);

        bool any_can_node_stale = false;
        for (std::size_t i = 0u; i < datalogger::bsw::VehicleCanState::kNodeCount; ++i) {
            const auto node = static_cast<datalogger::bsw::CanNodeId>(i);
            if (can_state.node_ever_received[i] &&
                !ctx->can_decoder->is_node_online(node, now_ms)) {
                any_can_node_stale = true;
                break;
            }
        }

        ctx->shared->latest_ecu_rpm.store(can_state.ecu.rpm, std::memory_order_relaxed);
        ctx->shared->ecu_online.store(ecu_on, std::memory_order_relaxed);
        ctx->shared->can_warning.store(any_can_node_stale, std::memory_order_relaxed);

        if (ctx->shared->recording_requested.load(std::memory_order_relaxed)) {
            const uint16_t beacon = ctx->shared->lap_beacon.load(std::memory_order_relaxed);
            ctx->aggregator->publish_snapshot(*ctx->telemetry_queue, 0u, beacon);
        }

        vTaskDelayUntil(&last_wake_time, kAcquisitionPeriodTicks);
    }
}

/**
 * @brief ASW Task da Interface de Pista e Máquina de Estados 50 Hz (FreeRTOS Core 0 - PRO_CPU)
 * Avalia acionamento automático por RPM (> 600 RPM + retenção de 5 s), lê `Free_SW2` (Lap Beacon)
 * com debounce (`Free_SW1` em standby) e comanda os LEDs `LED_Acquiring` e `LED_Working`.
 */
void ui_task(void* pvParameters) {
    auto* ctx = static_cast<UiTaskContext*>(pvParameters);
    TickType_t last_wake_time = xTaskGetTickCount();

    while (true) {
        datalogger::bsw::UiSystemInputs inputs{};
        inputs.now_ms = ctx->rtc_service->sample_timestamp().monotonic_ms;
        inputs.ecu_rpm = ctx->shared->latest_ecu_rpm.load(std::memory_order_relaxed);
        inputs.ecu_online = ctx->shared->ecu_online.load(std::memory_order_relaxed);
        inputs.session_active = ctx->shared->session_active.load(std::memory_order_relaxed);
        inputs.storage_fault = ctx->shared->storage_fault.load(std::memory_order_relaxed);
        inputs.can_warning = ctx->shared->can_warning.load(std::memory_order_relaxed);

        ctx->ui_service->update(inputs);
        ctx->engine_hours_service->update(inputs.now_ms, inputs.ecu_rpm, inputs.ecu_online);

        ctx->shared->recording_requested.store(ctx->ui_service->recording_requested(),
                                               std::memory_order_relaxed);
        ctx->shared->lap_beacon.store(ctx->ui_service->lap_beacon(),
                                      std::memory_order_relaxed);
        ctx->shared->engine_hours_total_s.store(ctx->engine_hours_service->total_seconds(),
                                                std::memory_order_relaxed);

        vTaskDelayUntil(&last_wake_time, kUiPeriodTicks);
    }
}

/**
 * @brief ASW Task de Armazenamento MicroSD (FreeRTOS Core 0 - PRO_CPU)
 * Abre/fecha sessões `.bin` (`PDLG v1`) sob comando da máquina de estados de RPM e
 * grava blocos de 4 KB com `fsync` periódico.
 */
void storage_logger_task(void* pvParameters) {
    auto* ctx = static_cast<StorageTaskContext*>(pvParameters);
    uint64_t last_open_retry_ms = 0ULL;
    bool retry_cooldown_active = false;
    datalogger::bsw::TelemetrySnapshot discard{};

    while (true) {
        const bool want_record = ctx->shared->recording_requested.load(std::memory_order_relaxed);
        const bool active = ctx->logger_service->is_session_active();

        if (want_record && !active) {
            const uint64_t now_ms = ctx->rtc_service->sample_timestamp().monotonic_ms;
            if (!retry_cooldown_active ||
                (now_ms >= last_open_retry_ms &&
                 (now_ms - last_open_retry_ms) >= kStorageRetryCooldownMs)) {
                while (ctx->telemetry_queue->receive(discard, 0u)) {}
                const uint32_t eng_sec =
                    ctx->shared->engine_hours_total_s.load(std::memory_order_relaxed);
                if (ctx->logger_service->start_session(*ctx->rtc_service, eng_sec)) {
                    retry_cooldown_active = false;
                    ctx->shared->storage_fault.store(false, std::memory_order_relaxed);
                    ctx->shared->session_active.store(true, std::memory_order_relaxed);
                    ESP_LOGI(kTag,
                             "Sessao MicroSD iniciada (RPM > 600, motor=%02lu:%02lu): %.*s",
                             static_cast<unsigned long>(eng_sec / 3600u),
                             static_cast<unsigned long>((eng_sec % 3600u) / 60u),
                             static_cast<int>(ctx->logger_service->current_file_path().size()),
                             ctx->logger_service->current_file_path().data());
                } else {
                    retry_cooldown_active = true;
                    last_open_retry_ms = now_ms;
                    ctx->shared->storage_fault.store(true, std::memory_order_relaxed);
                    ctx->shared->session_active.store(false, std::memory_order_relaxed);
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
            continue;
        }

        if (!want_record && active) {
            for (int i = 0; i < 32 && ctx->logger_service->process_queue_step(*ctx->telemetry_queue, 0u); ++i) {}
            const bool stopped_ok = ctx->logger_service->stop_session();
            ctx->shared->session_active.store(false, std::memory_order_relaxed);
            if (!stopped_ok) {
                ctx->shared->storage_fault.store(true, std::memory_order_relaxed);
            }
            retry_cooldown_active = false;
            const uint32_t eng_sec =
                ctx->shared->engine_hours_total_s.load(std::memory_order_relaxed);
            (void)eng_sec;
            ESP_LOGI(kTag,
                     "Sessao MicroSD encerrada apos 5s de motor parado (amostras=%lu, motor=%02lu:%02lu)",
                     static_cast<unsigned long>(ctx->logger_service->samples_logged()),
                     static_cast<unsigned long>(eng_sec / 3600u),
                     static_cast<unsigned long>((eng_sec % 3600u) / 60u));
            continue;
        }

        if (ctx->logger_service->is_session_active()) {
            const uint32_t prev_errors = ctx->logger_service->storage_error_count();
            ctx->logger_service->process_queue_step(*ctx->telemetry_queue, 50u);
            if (ctx->logger_service->storage_error_count() > prev_errors) {
                ctx->shared->storage_fault.store(true, std::memory_order_relaxed);
            }
        } else {
            while (ctx->telemetry_queue->receive(discard, 0u)) {}
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

} // namespace

extern "C" void app_main(void) {
    ESP_LOGI(kTag, "Inicializando Firmware Datalogger Central ESP32-S3 (Arquitetura Motorsport)...");

    static SharedRuntimeState shared_state{};

    static datalogger::osal::FreeRtosQueueAdapter<datalogger::hal::CanFrame> can_rx_queue(64);
    static datalogger::osal::FreeRtosQueueAdapter<datalogger::bsw::TelemetrySnapshot> telemetry_queue(32);

    static datalogger::lld::NvsLld nvs_lld;
    static datalogger::bsw::EngineHoursService engine_hours_service(nvs_lld);

    static datalogger::lld::GpioLld gpio_lld;
    static datalogger::bsw::UiService ui_service(gpio_lld);

    static datalogger::lld::CanTwaiLld can_twai_lld;
    static datalogger::bsw::CanService can_service(can_twai_lld);
    static datalogger::bsw::CanDecoderService can_decoder;

    static datalogger::lld::AdcOneshotLld adc_oneshot_lld;
    static datalogger::bsw::AdcService adc_service(adc_oneshot_lld);

    static datalogger::lld::Ds3231I2cLld ds3231_lld;
    static datalogger::bsw::RtcService rtc_service(ds3231_lld);

    static datalogger::lld::Neo8mUartLld neo8m_lld;
    static datalogger::bsw::GpsService gps_service(neo8m_lld, rtc_service);

    static datalogger::bsw::TelemetryAggregator telemetry_aggregator(
        rtc_service, can_decoder, adc_service, gps_service);

    static datalogger::lld::SdCardLld sd_card_lld;
    static datalogger::bsw::LoggerService logger_service(sd_card_lld);

    // Inicializa horímetro permanente do motor na partição NVS (checkpoint 1 min + parada)
    {
        const bool nvm_ok = engine_hours_service.initialize();
        shared_state.engine_hours_total_s.store(engine_hours_service.total_seconds(),
                                                std::memory_order_relaxed);
        std::array<char, datalogger::bsw::EngineHoursService::kMinFormatBufSize> hhmm_buf{};
        engine_hours_service.format_hhmm(hhmm_buf.data(), hhmm_buf.size());
        if (nvm_ok) {
            ESP_LOGI(kTag, "Horimetro de motor (NVS): %s (%lu s acumulados)",
                     hhmm_buf.data(),
                     static_cast<unsigned long>(engine_hours_service.total_seconds()));
        } else {
            ESP_LOGW(kTag, "Falha ao abrir NVS do horimetro; acumulando apenas em RAM nesta sessao");
        }
    }

    // Inicializa interface de pista (Free_SW1 standby, Free_SW2 beacon, LEDs Acquiring/Working)
    if (!ui_service.initialize()) {
        ESP_LOGW(kTag, "Falha ao inicializar BSW UiService / LLD GPIO!");
    }

    // Inicializa com a Pinning Table do BSP (GPIO 15 TX, GPIO 16 RX @ 500 kbps)
    if (!can_service.initialize()) {
        ESP_LOGE(kTag, "Falha ao inicializar BSW CanService / LLD SN65HVD230DR!");
        return;
    }

    // Inicializa aquisição analógica local (ADC1: 10 canais em GPIO 1..10)
    if (!adc_service.initialize()) {
        ESP_LOGE(kTag, "Falha ao inicializar BSW AdcService / LLD ADC1!");
        return;
    }

    // Inicializa RTC externo DS3231SN# (I2C GPIO 11/12, 0x68 @ 400 kHz); falha mantém modo degradado 1 ms
    if (rtc_service.initialize()) {
        ESP_LOGI(kTag, "RTC DS3231SN# inicializado e sincronizado (base 1 ms)");
    } else {
        ESP_LOGW(kTag,
                 "Falha ao sincronizar RTC DS3231SN# (status=%u); operando em modo degradado monotonico (1 ms)",
                 static_cast<unsigned>(rtc_service.status()));
    }

    // Inicializa GPS Neo-8M (UART GPIO 13 RX / GPIO 47 TX, 115200 baud @ 10 Hz); falha não aborta aquisição
    if (gps_service.initialize()) {
        ESP_LOGI(kTag, "GPS Neo-8M inicializado (UART 115200 baud, 10 Hz)");
    } else {
        ESP_LOGW(kTag, "Falha ao inicializar GPS Neo-8M; seguindo aquisicao em modo degradado sem GPS");
    }

    // Monta o MicroSD J4 e permanece em Standby armado aguardando ECU RPM > 600 para iniciar sessão .bin
    if (logger_service.initialize()) {
        shared_state.storage_fault.store(false, std::memory_order_relaxed);
        ESP_LOGI(kTag, "MicroSD montado; sistema em Standby armado aguardando RPM > 600");
    } else {
        shared_state.storage_fault.store(true, std::memory_order_relaxed);
        ESP_LOGW(kTag, "Falha ao montar MicroSD no boot; LED_Working sinalizara falha ate cartao ficar disponivel");
    }

    static CanRxTaskContext can_rx_ctx{&can_service, &can_rx_queue};
    static AcquisitionTaskContext acq_ctx{
        &can_rx_queue,
        &can_decoder,
        &adc_service,
        &gps_service,
        &rtc_service,
        &telemetry_aggregator,
        &telemetry_queue,
        &shared_state
    };
    static UiTaskContext ui_ctx{
        &ui_service,
        &engine_hours_service,
        &rtc_service,
        &shared_state
    };
    static StorageTaskContext storage_ctx{
        &logger_service,
        &rtc_service,
        &telemetry_queue,
        &shared_state
    };

    if (xTaskCreatePinnedToCore(
            can_rx_task,
            "can_rx_task",
            4096,
            &can_rx_ctx,
            configMAX_PRIORITIES - 2,
            nullptr,
            1) != pdPASS) {
        ESP_LOGE(kTag, "Falha ao criar can_rx_task no Core 1");
    }

    if (xTaskCreatePinnedToCore(
            acquisition_task,
            "acq_task",
            4096,
            &acq_ctx,
            configMAX_PRIORITIES - 3,
            nullptr,
            1) != pdPASS) {
        ESP_LOGE(kTag, "Falha ao criar acq_task no Core 1");
    }

    if (xTaskCreatePinnedToCore(
            ui_task,
            "ui_task",
            3072,
            &ui_ctx,
            tskIDLE_PRIORITY + 3,
            nullptr,
            0) != pdPASS) {
        ESP_LOGE(kTag, "Falha ao criar ui_task no Core 0");
    }

    if (xTaskCreatePinnedToCore(
            storage_logger_task,
            "storage_task",
            6144,
            &storage_ctx,
            tskIDLE_PRIORITY + 2,
            nullptr,
            0) != pdPASS) {
        ESP_LOGE(kTag, "Falha ao criar storage_task no Core 0");
    }
}
