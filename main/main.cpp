#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "bsp_pins.h"
#include "can_decoder_service.hpp"
#include "can_service.hpp"
#include "can_twai_lld.hpp"
#include "freertos_queue_adapter.hpp"

namespace {

constexpr const char* kTag = "ASW_MAIN";

struct CanRxTaskContext {
    datalogger::bsw::CanService* service;
    datalogger::osal::IOsalQueue<datalogger::hal::CanFrame>* rx_queue;
};

struct CanDecodeTaskContext {
    datalogger::osal::IOsalQueue<datalogger::hal::CanFrame>* rx_queue;
    datalogger::bsw::CanDecoderService* decoder;
};

/**
 * @brief ASW Task de Aquisição CAN (FreeRTOS Core 1)
 * Lê frames do LLD (SN65HVD230DR) via BSW CanService e enfileira na OSAL Queue.
 */
void can_rx_task(void* pvParameters) {
    auto* ctx = static_cast<CanRxTaskContext*>(pvParameters);

    while (true) {
        ctx->service->check_and_recover_bus();
        ctx->service->receive_to_queue(*ctx->rx_queue, 100, 10);
    }
}

/**
 * @brief ASW Task Consumidora / Decodificadora CAN (FreeRTOS Core 0)
 * Consome os frames da OSAL Queue e atualiza a tabela VehicleCanState via CanDecoderService.
 */
void can_logger_task(void* pvParameters) {
    auto* ctx = static_cast<CanDecodeTaskContext*>(pvParameters);
    datalogger::hal::CanFrame frame{};

    while (true) {
        if (ctx->rx_queue->receive(frame, 500)) {
            if (ctx->decoder->decode_frame(frame)) {
                const auto& st = ctx->decoder->state();
                (void)st;
                ESP_LOGI(kTag, "[CAN DECODED] ID=0x%08lX | RPM=%u Gear=%d Steer=%.1f deg",
                         static_cast<unsigned long>(frame.id),
                         static_cast<unsigned>(st.ecu.rpm),
                         static_cast<int>(st.ecu.gear),
                         static_cast<double>(st.front_controls.steering_angle_deg));
            }
        }
    }
}

} // namespace

extern "C" void app_main(void) {
    ESP_LOGI(kTag, "Inicializando Firmware Datalogger Central ESP32-S3 (Arquitetura Motorsport)...");

    static datalogger::osal::FreeRtosQueueAdapter<datalogger::hal::CanFrame> can_rx_queue(64);
    static datalogger::lld::CanTwaiLld can_twai_lld;
    static datalogger::bsw::CanService can_service(can_twai_lld);
    static datalogger::bsw::CanDecoderService can_decoder;

    // Inicializa com a Pinning Table do BSP (GPIO 15 TX, GPIO 16 RX @ 500 kbps)
    if (!can_service.initialize()) {
        ESP_LOGE(kTag, "Falha ao inicializar BSW CanService / LLD SN65HVD230DR!");
        return;
    }

    static CanRxTaskContext rx_ctx{&can_service, &can_rx_queue};
    static CanDecodeTaskContext decode_ctx{&can_rx_queue, &can_decoder};

    xTaskCreatePinnedToCore(
        can_rx_task,
        "can_rx_task",
        4096,
        &rx_ctx,
        configMAX_PRIORITIES - 2,
        nullptr,
        1 // Core 1 (APP_CPU): Aquisição determinística em tempo real
    );

    xTaskCreatePinnedToCore(
        can_logger_task,
        "can_log_task",
        4096,
        &decode_ctx,
        tskIDLE_PRIORITY + 2,
        nullptr,
        0 // Core 0 (PRO_CPU): Decodificação, gravação e serviços de sistema
    );
}
