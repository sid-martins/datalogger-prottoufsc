#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "i_osal_queue.hpp"

namespace datalogger::osal {

/**
 * @brief Adaptador concreto da camada OSAL sobre `xQueue` do FreeRTOS (ESP-IDF).
 */
template <typename T>
class FreeRtosQueueAdapter : public IOsalQueue<T> {
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

} // namespace datalogger::osal

