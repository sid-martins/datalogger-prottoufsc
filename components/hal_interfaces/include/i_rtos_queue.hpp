#pragma once

#include <cstdint>

namespace datalogger::hal {

/**
 * @brief Abstração de fila do FreeRTOS (xQueueSend / xQueueReceive) para permitir
 * testar a comunicação produtor-consumidor entre tasks do FreeRTOS via TDD no Host.
 */
template <typename T>
class IRtosQueue {
public:
    virtual ~IRtosQueue() = default;

    /**
     * @brief Envia um item para a fila.
     * @param item Item a ser copiado para a fila.
     * @param timeout_ms Tempo máximo de espera em milissegundos.
     * @return true se o item foi enfileirado, false se a fila estava cheia (timeout).
     */
    virtual bool send(const T& item, uint32_t timeout_ms) = 0;

    /**
     * @brief Recebe um item da fila.
     * @param[out] out_item Variável que receberá o item desenfileirado.
     * @param timeout_ms Tempo máximo de espera em milissegundos.
     * @return true se um item foi recebido, false se a fila estava vazia (timeout).
     */
    virtual bool receive(T& out_item, uint32_t timeout_ms) = 0;

    /**
     * @brief Retorna o número de mensagens atualmente aguardando na fila.
     */
    virtual uint32_t messages_waiting() const = 0;
};

} // namespace datalogger::hal

