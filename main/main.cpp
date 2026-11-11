#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "board_pins.h"
#include "datalogger_service.hpp"
#include "i_rtos_queue.hpp"
#include "i_sensor.hpp"
#include "i_storage.hpp"

namespace {

constexpr const char* kTag = "DATALOGGER_MAIN";

/**
 * @brief Implementação real da fila do FreeRTOS para o ESP32-S3.
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

/**
 * @brief Exemplo de driver de sensor no ESP32-S3 (substitua pelo seu driver I2C/SPI/ADC real).
 */
class Esp32SimulatedSensor : public datalogger::hal::ISensor {
public:
    bool init() override {
        ESP_LOGI(kTag, "Sensor inicializado no ESP32-S3");
        return true;
    }

    bool read(datalogger::hal::SensorReading& out_reading) override {
        out_reading.timestamp_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
        out_reading.temperature_c = 25.4f;
        out_reading.humidity_pct = 61.2f;
        out_reading.battery_mv = 3720.0f;
        return true;
    }
};

/**
 * @brief Exemplo de driver de Storage no ESP32-S3 (substitua por SDMMC/SPI SD ou FATFS/LittleFS).
 */
class Esp32ConsoleStorage : public datalogger::hal::IStorage {
public:
    bool mount() override {
        ESP_LOGI(kTag, "Storage montado (/storage)");
        return true;
    }

    bool append_line(std::string_view file_path, std::string_view line) override {
        ESP_LOGI(kTag, "[%.*s] %.*s",
                 static_cast<int>(file_path.size()), file_path.data(),
                 static_cast<int>(line.size()), line.data());
        return true;
    }

    bool sync() override {
        ESP_LOGI(kTag, "Storage fsync concluido");
        return true;
    }
};

/**
 * @brief Task Produtora (FreeRTOS) - Amostragem periódica dos sensores.
 * Fixada no Core 1 (APP_CPU) do ESP32-S3 para determinismo.
 */
void sensor_acquisition_task(void* pvParameters) {
    auto* service = static_cast<datalogger::DataloggerService*>(pvParameters);
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t sample_period_ticks = pdMS_TO_TICKS(1000); // 1 Hz

    while (true) {
        if (!service->acquisition_step(50)) {
            ESP_LOGW(kTag, "Falha na aquisicao (erros=%lu, drops=%lu)",
                     static_cast<unsigned long>(service->read_errors()),
                     static_cast<unsigned long>(service->dropped_samples()));
        }
        vTaskDelayUntil(&last_wake_time, sample_period_ticks);
    }
}

/**
 * @brief Task Consumidora (FreeRTOS) - Persistência em armazenamento não-volátil.
 * Fixada no Core 0 (PRO_CPU) do ESP32-S3.
 */
void storage_writer_task(void* pvParameters) {
    auto* service = static_cast<datalogger::DataloggerService*>(pvParameters);

    while (true) {
        service->storage_step(500);
    }
}

} // namespace

extern "C" void app_main(void) {
    ESP_LOGI(kTag, "Inicializando Firmware Datalogger ESP32-S3...");
    ESP_LOGI(kTag, "Pino I2C SDA: GPIO %d (%s) | SCL: GPIO %d (%s)",
             static_cast<int>(PIN_I2C_SDA), TAG_I2C_SDA,
             static_cast<int>(PIN_I2C_SCL), TAG_I2C_SCL);

    static FreeRtosQueueAdapter<datalogger::hal::SensorReading> sample_queue(32);
    static Esp32SimulatedSensor sensor;
    static Esp32ConsoleStorage storage;
    static datalogger::DataloggerService datalogger_service(
        sensor, storage, sample_queue, "/storage/datalog.csv", 5);

    if (!datalogger_service.initialize()) {
        ESP_LOGE(kTag, "Falha critica ao inicializar o DataloggerService!");
        return;
    }

    // Criação das Tasks no FreeRTOS (Dual-Core ESP32-S3)
    xTaskCreatePinnedToCore(
        sensor_acquisition_task,
        "acq_task",
        4096,
        &datalogger_service,
        configMAX_PRIORITIES - 2,
        nullptr,
        1 // Core 1: Aquisição em tempo real
    );

    xTaskCreatePinnedToCore(
        storage_writer_task,
        "storage_task",
        4096,
        &datalogger_service,
        tskIDLE_PRIORITY + 2,
        nullptr,
        0 // Core 0: I/O de escrita em cartão SD / Flash
    );
}

