#pragma once

#include <queue>
#include <gmock/gmock.h>
#include "i_rtos_queue.hpp"

namespace datalogger::testing {

/**
 * @brief Fake funcional de uma fila do FreeRTOS em memória (std::queue com limite de capacidade)
 * para testar cenários de fila cheia, fila vazia e comunicação produtor-consumidor entre tasks.
 */
template <typename T>
class FakeRtosQueue : public hal::IRtosQueue<T> {
public:
    explicit FakeRtosQueue(std::size_t max_capacity = 16)
        : max_capacity_(max_capacity) {}

    bool send(const T& item, uint32_t timeout_ms) override {
        last_send_timeout_ms_ = timeout_ms;
        if (queue_.size() >= max_capacity_) {
            return false; // Simula errQUEUE_FULL após timeout no FreeRTOS
        }
        queue_.push(item);
        return true;
    }

    bool receive(T& out_item, uint32_t timeout_ms) override {
        last_receive_timeout_ms_ = timeout_ms;
        if (queue_.empty()) {
            return false; // Simula errQUEUE_EMPTY após timeout no FreeRTOS
        }
        out_item = queue_.front();
        queue_.pop();
        return true;
    }

    uint32_t messages_waiting() const override {
        return static_cast<uint32_t>(queue_.size());
    }

    uint32_t last_send_timeout_ms() const { return last_send_timeout_ms_; }
    uint32_t last_receive_timeout_ms() const { return last_receive_timeout_ms_; }

private:
    std::size_t max_capacity_;
    std::queue<T> queue_;
    uint32_t last_send_timeout_ms_{0};
    uint32_t last_receive_timeout_ms_{0};
};

} // namespace datalogger::testing

