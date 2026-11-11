#pragma once

#include <array>
#include <cstddef>

namespace datalogger {

/**
 * @brief Buffer circular estático genérico (sem alocação dinâmica de memória / heap)
 * para armazenar frames CAN, amostras de ADC ou pacotes de log no ESP32-S3.
 *
 * @tparam T Tipo do elemento armazenado (ex: hal::CanFrame).
 * @tparam Capacity Capacidade máxima de elementos no buffer.
 */
template <typename T, std::size_t Capacity>
class RingBuffer {
public:
    static_assert(Capacity > 0, "RingBuffer capacity must be greater than 0");

    RingBuffer() = default;

    /**
     * @brief Insere um elemento no buffer.
     * @param item Elemento a ser inserido.
     * @param overwrite Se true, sobrescreve o elemento mais antigo quando cheio.
     * @return true se inserido com sucesso, false se cheio e overwrite == false.
     */
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

    /**
     * @brief Remove e retorna o elemento mais antigo do buffer (FIFO).
     * @param[out] out_item Recebe o elemento removido.
     * @return true se havia elemento disponível, false se o buffer estava vazio.
     */
    bool pop(T& out_item) {
        if (empty()) {
            return false;
        }
        out_item = buffer_[tail_];
        tail_ = (tail_ + 1) % Capacity;
        --count_;
        return true;
    }

    /**
     * @brief Limpa todos os elementos do buffer.
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
    std::array<T, Capacity> buffer_{};
    std::size_t head_{0};
    std::size_t tail_{0};
    std::size_t count_{0};
};

} // namespace datalogger
