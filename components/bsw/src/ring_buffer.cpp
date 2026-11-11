#include "ring_buffer.hpp"
#include "i_can_hal.hpp"

namespace datalogger::bsw {
template class RingBuffer<hal::CanFrame, 16>;
template class RingBuffer<hal::CanFrame, 64>;
} // namespace datalogger::bsw

