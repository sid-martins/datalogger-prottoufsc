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
#include "freertos_queue_adapter.hpp"
#include "gps_service.hpp"
#include "logger_service.hpp"
#include "neo8m_uart_lld.hpp"
#include "rtc_service.hpp"
#include "sd_card_lld.hpp"
#include "telemetry_aggregator.hpp"
#include "telemetry_snapshot.hpp"

namespace {

constexpr const char* kTag = "ASW_MAIN";
constexpr TickType_t kAcquisitionPeriodTicks = pdMS_TO_TICKS(10); // 100 Hz

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
};

struct StorageTaskContext {
    datalogger::bsw::LoggerService* logger_service;
    datalogger::osal::IOsalQueue<datalogger::bsw::TelemetrySnapshot>* telemetry_queue;
};

/**
 * @brief ASW Task de Recepção CAN (FreeRTOS Core 1 - APP_CPU)
 * Lê frames do transceptor SN65HVD230DR e os enfileira sem bloqueio.
 */
void can_rx_task(void* pvParameters) {
    auto* ctx = static_cast<CanRxTaskContext*>(pvParameters);

    while (true) {
        ctx->service->check_and_recover_bus();
        ctx->service->receive_to_queue(*ctx->rx_queue, 100, 10);
    }
}

/**
 * @brief ASW Task Determinística de Aquisição 100 Hz (FreeRTOS Core 1 - APP_CPU)
 * Drena frames CAN para o decodificador, amostra ADC1 (multi-taxa), lê GPS e
 * publica snapshots unificados para a fila do Core 0.
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
        ctx->aggregator->publish_snapshot(*ctx->telemetry_queue, 0u);

        vTaskDelayUntil(&last_wake_time, kAcquisitionPeriodTicks);
    }
}

/**
 * @brief ASW Task de Armazenamento MicroSD (FreeRTOS Core 0 - PRO_CPU)
 * Consome snapshots da fila OSAL e grava registros binários `.bin` (`PDLG v1`) em blocos de 4 KB com fsync periódico.
 */
void storage_logger_task(void* pvParameters) {
    auto* ctx = static_cast<StorageTaskContext*>(pvParameters);

    while (true) {
        if (!ctx->logger_service->is_session_active()) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        ctx->logger_service->process_queue_step(*ctx->telemetry_queue, 200u);
    }
}

} // namespace

extern "C" void app_main(void) {
    ESP_LOGI(kTag, "Inicializando Firmware Datalogger Central ESP32-S3 (Arquitetura Motorsport)...");

    static datalogger::osal::FreeRtosQueueAdapter<datalogger::hal::CanFrame> can_rx_queue(64);
    static datalogger::osal::FreeRtosQueueAdapter<datalogger::bsw::TelemetrySnapshot> telemetry_queue(32);

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

    // Inicializa MicroSD J4 e abre sessão binária .bin (PDLG v1); falha mantém aquisição ativa em modo degradado
    if (logger_service.initialize() && logger_service.start_session(rtc_service)) {
        ESP_LOGI(kTag, "Sessao MicroSD iniciada");
    } else {
        ESP_LOGW(kTag, "Falha ao inicializar/abrir sessao MicroSD; aquisicao continua sem gravacao em disco");
    }

    static CanRxTaskContext can_rx_ctx{&can_service, &can_rx_queue};
    static AcquisitionTaskContext acq_ctx{
        &can_rx_queue,
        &can_decoder,
        &adc_service,
        &gps_service,
        &rtc_service,
        &telemetry_aggregator,
        &telemetry_queue
    };
    static StorageTaskContext storage_ctx{&logger_service, &telemetry_queue};

    xTaskCreatePinnedToCore(
        can_rx_task,
        "can_rx_task",
        4096,
        &can_rx_ctx,
        configMAX_PRIORITIES - 2,
        nullptr,
        1 // Core 1 (APP_CPU): Recepção CAN em tempo real
    );

    xTaskCreatePinnedToCore(
        acquisition_task,
        "acq_task",
        4096,
        &acq_ctx,
        configMAX_PRIORITIES - 3,
        nullptr,
        1 // Core 1 (APP_CPU): Decodificação, ADC, GPS e publicação de snapshots (100 Hz)
    );

    xTaskCreatePinnedToCore(
        storage_logger_task,
        "storage_task",
        6144,
        &storage_ctx,
        tskIDLE_PRIORITY + 2,
        nullptr,
        0 // Core 0 (PRO_CPU): Gravação binária em bloco de 4 KB e fsync no MicroSD
    );
}
