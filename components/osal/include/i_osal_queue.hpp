#pragma once

#include <cstdint>

namespace datalogger::osal {

/**
 * @brief OSAL (Operating System Abstraction Layer) - Interface de fila do RTOS.
 *
 * Permite que os serviços da camada BSW/ASW troquem mensagens entre tasks do FreeRTOS
 * no ESP32-S3 e sejam validados em simulações SIL (Software-in-the-Loop) no PC.
 */
template <typename T>
class IOsalQueue {
public:
    virtual ~IOsalQueue() = default;

    virtual bool send(const T& item, uint32_t timeout_ms) = 0;
    virtual bool receive(T& out_item, uint32_t timeout_ms) = 0;
    virtual uint32_t messages_waiting() const = 0;
};

} // namespace datalogger::osal

