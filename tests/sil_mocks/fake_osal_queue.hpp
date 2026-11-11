#pragma once

#include <queue>
#include "i_osal_queue.hpp"

namespace datalogger::sil {

/**
 * @brief SIL (Software-in-the-Loop) Fake de uma fila da camada OSAL em memória.
 */
template <typename T>
class FakeOsalQueue : public osal::IOsalQueue<T> {
public:
    explicit FakeOsalQueue(std::size_t max_capacity = 16)
        : max_capacity_(max_capacity) {}

    bool send(const T& item, uint32_t timeout_ms) override {
        last_send_timeout_ms_ = timeout_ms;
        if (queue_.size() >= max_capacity_) {
            return false;
        }
        queue_.push(item);
        return true;
    }

    bool receive(T& out_item, uint32_t timeout_ms) override {
        last_receive_timeout_ms_ = timeout_ms;
        if (queue_.empty()) {
            return false;
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

} // namespace datalogger::sil

