#pragma once

#include <array>
#include <cstddef>
#include "i_sensor.hpp"

namespace datalogger {

/**
 * @brief Buffer circular estático (sem alocação dinâmica de memória / heap)
 * para armazenar leituras de sensores antes da gravação em lote na Flash/SD.
 *
 * @tparam Capacity Capacidade máxima de amostras no buffer.
 */
template <std::size_t Capacity>
class SampleRingBuffer {
public:
    static_assert(Capacity > 0, "SampleRingBuffer capacity must be greater than 0");

    SampleRingBuffer() = default;

    /**
     * @brief Insere uma amostra no buffer.
     * @param sample Amostra a ser inserida.
     * @param overwrite Se true, sobrescreve a amostra mais antiga quando cheio.
     * @return true se inserido com sucesso, false se cheio e overwrite == false.
     */
    bool push(const hal::SensorReading& sample, bool overwrite = false) {
        if (full()) {
            if (!overwrite) {
                return false;
            }
            tail_ = (tail_ + 1) % Capacity;
            --count_;
        }
        buffer_[head_] = sample;
        head_ = (head_ + 1) % Capacity;
        ++count_;
        return true;
    }

    /**
     * @brief Remove e retorna a amostra mais antiga do buffer (FIFO).
     * @param[out] out_sample Recebe a amostra removida.
     * @return true se havia amostra disponível, false se o buffer estava vazio.
     */
    bool pop(hal::SensorReading& out_sample) {
        if (empty()) {
            return false;
        }
        out_sample = buffer_[tail_];
        tail_ = (tail_ + 1) % Capacity;
        --count_;
        return true;
    }

    /**
     * @brief Limpa todas as amostras do buffer.
     */
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
    std::array<hal::SensorReading, Capacity> buffer_{};
    std::size_t head_{0};
    std::size_t tail_{0};
    std::size_t count_{0};
};

} // namespace datalogger

