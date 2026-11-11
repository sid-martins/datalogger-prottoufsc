#include "ring_buffer.hpp"
#include "i_can_driver.hpp"

namespace datalogger {
// Instanciações explícitas comuns para validação de compilação
template class RingBuffer<hal::CanFrame, 16>;
template class RingBuffer<hal::CanFrame, 64>;
} // namespace datalogger
