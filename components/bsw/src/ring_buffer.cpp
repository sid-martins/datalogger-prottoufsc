#include "ring_buffer.hpp"
#include "i_can_hal.hpp"
#include "telemetry_snapshot.hpp"

namespace datalogger::bsw {
template class RingBuffer<hal::CanFrame, 16>;
template class RingBuffer<hal::CanFrame, 64>;
template class RingBuffer<TelemetrySnapshot, 32>;
} // namespace datalogger::bsw
