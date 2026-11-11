#include "ring_buffer.hpp"

namespace datalogger {
// Instanciações explícitas comuns para validação de compilação da unidade de tradução
template class SampleRingBuffer<16>;
template class SampleRingBuffer<64>;
} // namespace datalogger

