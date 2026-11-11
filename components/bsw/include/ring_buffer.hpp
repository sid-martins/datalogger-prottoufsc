#pragma once

#include <array>
#include <cstddef>

namespace datalogger::bsw {

/**
 * @brief BSW (Basic Software) - Buffer circular estático genérico (sem alocação dinâmica / heap)
 * para armazenar frames CAN, amostras de ADC ou pacotes de telemetria em tempo real.
 */
template <typename T, std::size_t Capacity>
class RingBuffer {
public:
    static_assert(Capacity > 0, "RingBuffer capacity must be greater than 0");

    RingBuffer() = default;

    bool push(const T& item, bool overwrite = false) {
        if (full()) {
            if (!overwrite) {
                return false;
            }
            tail_ = (tail_ + 1) % Capacity;
            --count_;
        }
        buffer_[head_] = item;
        head_ = (head_ + 1) % Capacity;
        ++count_;
        return true;
    }

    bool pop(T& out_item) {
        if (empty()) {
            return false;
        }
        out_item = buffer_[tail_];
        tail_ = (tail_ + 1) % Capacity;
        --count_;
        return true;
    }

    void clear() {
        head_ = 0;
        tail_ = 0;
        count_ = 0;
    }

    bool empty() const { return count_ == 0; }
    bool full() const { return count_ == Capacity; }
    std::size_t size() const { return count_; }
    constexpr std::size_t capacity() const { return Capacity; }

private:
    std::array<T, Capacity> buffer_{};
    std::size_t head_{0};
    std::size_t tail_{0};
    std::size_t count_{0};
};

} // namespace datalogger::bsw

