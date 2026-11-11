#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "bsp_pins.h"
#include "can_service.hpp"
#include "can_twai_lld.hpp"
#include "freertos_queue_adapter.hpp"

namespace {

constexpr const char* kTag = "ASW_MAIN";

struct CanTaskContext {
    datalogger::bsw::CanService* service;
    datalogger::osal::IOsalQueue<datalogger::hal::CanFrame>* rx_queue;
};

/**
 * @brief ASW Task de Aquisição CAN (FreeRTOS Core 1)
 * Lê frames do LLD (SN65HVD230DR) via BSW CanService e enfileira na OSAL Queue.
 */
void can_rx_task(void* pvParameters) {
    auto* ctx = static_cast<CanTaskContext*>(pvParameters);

    while (true) {
        ctx->service->check_and_recover_bus();
        ctx->service->receive_to_queue(*ctx->rx_queue, 100, 10);
    }
}

/**
 * @brief ASW Task Consumidora / Datalogger (FreeRTOS Core 0)
 * Consome os frames da OSAL Queue para persistência e telemetria.
 */
void can_logger_task(void* pvParameters) {
    auto* rx_queue = static_cast<datalogger::osal::IOsalQueue<datalogger::hal::CanFrame>*>(pvParameters);
    datalogger::hal::CanFrame frame{};

    while (true) {
        if (rx_queue->receive(frame, 500)) {
            ESP_LOGI(kTag, "[CAN RX] ID=0x%08lX (%s) DLC=%u t=%llu ms",
                     static_cast<unsigned long>(frame.id),
                     frame.is_extended ? "EXT" : "STD",
                     static_cast<unsigned>(frame.dlc),
                     static_cast<unsigned long long>(frame.timestamp_ms));
        }
    }
}

} // namespace

extern "C" void app_main(void) {
    ESP_LOGI(kTag, "Inicializando Firmware Datalogger ESP32-S3 (Arquitetura Motorsport)...");

    static datalogger::osal::FreeRtosQueueAdapter<datalogger::hal::CanFrame> can_rx_queue(64);
    static datalogger::lld::CanTwaiLld can_twai_lld;
    static datalogger::bsw::CanService can_service(can_twai_lld);

    // Inicializa com a Pinning Table do BSP (GPIO 15 TX, GPIO 16 RX @ 500 kbps)
    if (!can_service.initialize()) {
        ESP_LOGE(kTag, "Falha ao inicializar BSW CanService / LLD SN65HVD230DR!");
        return;
    }

    static CanTaskContext can_ctx{&can_service, &can_rx_queue};

    xTaskCreatePinnedToCore(
        can_rx_task,
        "can_rx_task",
        4096,
        &can_ctx,
        configMAX_PRIORITIES - 2,
        nullptr,
        1 // Core 1 (APP_CPU): Aquisição determinística em tempo real
    );

    xTaskCreatePinnedToCore(
        can_logger_task,
        "can_log_task",
        4096,
        &can_rx_queue,
        tskIDLE_PRIORITY + 2,
        nullptr,
        0 // Core 0 (PRO_CPU): Gravação e serviços de sistema
    );
}
