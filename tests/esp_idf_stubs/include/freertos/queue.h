#pragma once

#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* QueueHandle_t;

inline QueueHandle_t xQueueCreate(const UBaseType_t uxQueueLength,
                                  const UBaseType_t uxItemSize) {
    (void)uxQueueLength;
    (void)uxItemSize;
    return reinterpret_cast<QueueHandle_t>(1);
}

inline void vQueueDelete(QueueHandle_t xQueue) { (void)xQueue; }

inline BaseType_t xQueueSend(QueueHandle_t xQueue,
                             const void* const pvItemToQueue,
                             TickType_t xTicksToWait) {
    (void)xQueue;
    (void)pvItemToQueue;
    (void)xTicksToWait;
    return pdTRUE;
}

inline BaseType_t xQueueReceive(QueueHandle_t xQueue,
                                void* const pvBuffer,
                                TickType_t xTicksToWait) {
    (void)xQueue;
    (void)pvBuffer;
    (void)xTicksToWait;
    return pdTRUE;
}

inline UBaseType_t uxQueueMessagesWaiting(const QueueHandle_t xQueue) {
    (void)xQueue;
    return 0;
}

#ifdef __cplusplus
}
#endif

