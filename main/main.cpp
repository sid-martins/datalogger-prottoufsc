#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "board_pins.h"
#include "can_controller.hpp"
#include "esp32_twai_driver.hpp"
#include "i_rtos_queue.hpp"

namespace {

constexpr const char* kTag = "DATALOGGER_MAIN";

/**
 * @brief Adaptador de fila real do FreeRTOS para comunicação entre tasks no ESP32-S3.
 */
template <typename T>
class FreeRtosQueueAdapter : public datalogger::hal::IRtosQueue<T> {
public:
    explicit FreeRtosQueueAdapter(UBaseType_t length) {
        handle_ = xQueueCreate(length, sizeof(T));
    }

    ~FreeRtosQueueAdapter() override {
        if (handle_ != nullptr) {
            vQueueDelete(handle_);
        }
    }

    bool send(const T& item, uint32_t timeout_ms) override {
        if (handle_ == nullptr) return false;
        return xQueueSend(handle_, &item, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
    }

    bool receive(T& out_item, uint32_t timeout_ms) override {
        if (handle_ == nullptr) return false;
        return xQueueReceive(handle_, &out_item, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
    }

    uint32_t messages_waiting() const override {
        if (handle_ == nullptr) return 0;
        return static_cast<uint32_t>(uxQueueMessagesWaiting(handle_));
    }

private:
    QueueHandle_t handle_{nullptr};
};

struct CanTaskContext {
    datalogger::can::CanController* controller;
    datalogger::hal::IRtosQueue<datalogger::hal::CanFrame>* rx_queue;
};

/**
 * @brief Task de Recepção CAN (FreeRTOS Core 1) - Lê frames do SN65HVD230DR e enfileira.
 */
void can_rx_task(void* pvParameters) {
    auto* ctx = static_cast<CanTaskContext*>(pvParameters);

    while (true) {
        ctx->controller->check_and_recover_bus();
        ctx->controller->receive_to_queue(*ctx->rx_queue, 100, 10);
    }
}

/**
 * @brief Task Consumidora (FreeRTOS Core 0) - Processa/grava os frames CAN recebidos.
 */
void can_logger_task(void* pvParameters) {
    auto* rx_queue = static_cast<datalogger::hal::IRtosQueue<datalogger::hal::CanFrame>*>(pvParameters);
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
    ESP_LOGI(kTag, "Inicializando Firmware Datalogger ESP32-S3...");

    static FreeRtosQueueAdapter<datalogger::hal::CanFrame> can_rx_queue(64);
    static datalogger::can::Esp32TwaiDriver twai_driver;
    static datalogger::can::CanController can_controller(twai_driver);

    // Inicializa com a configuração padrão do esquemático (GPIO 15 TX, GPIO 16 RX @ 500 kbps)
    if (!can_controller.initialize()) {
        ESP_LOGE(kTag, "Falha ao inicializar barramento CAN (SN65HVD230DR)!");
        return;
    }

    static CanTaskContext can_ctx{&can_controller, &can_rx_queue};

    xTaskCreatePinnedToCore(
        can_rx_task,
        "can_rx_task",
        4096,
        &can_ctx,
        configMAX_PRIORITIES - 2,
        nullptr,
        1 // Core 1: Aquisição em tempo real do barramento CAN
    );

    xTaskCreatePinnedToCore(
        can_logger_task,
        "can_log_task",
        4096,
        &can_rx_queue,
        tskIDLE_PRIORITY + 2,
        nullptr,
        0 // Core 0: Persistência e logs
    );
}
