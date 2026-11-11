#pragma once

#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* TaskHandle_t;
typedef void (*TaskFunction_t)(void*);

inline TickType_t xTaskGetTickCount(void) { return 0; }

inline void vTaskDelay(const TickType_t xTicksToDelay) { (void)xTicksToDelay; }

inline BaseType_t xTaskDelayUntil(TickType_t* const pxPreviousWakeTime,
                                  const TickType_t xTimeIncrement) {
    if (pxPreviousWakeTime != nullptr) {
        *pxPreviousWakeTime += xTimeIncrement;
    }
    return pdTRUE;
}

#define vTaskDelayUntil(pxPreviousWakeTime, xTimeIncrement) \
    (void)xTaskDelayUntil((pxPreviousWakeTime), (xTimeIncrement))

inline BaseType_t xTaskCreatePinnedToCore(TaskFunction_t pvTaskCode,
                                          const char* const pcName,
                                          const uint32_t usStackDepth,
                                          void* const pvParameters,
                                          UBaseType_t uxPriority,
                                          TaskHandle_t* const pvCreatedTask,
                                          const BaseType_t xCoreID) {
    (void)pvTaskCode;
    (void)pcName;
    (void)usStackDepth;
    (void)pvParameters;
    (void)uxPriority;
    (void)pvCreatedTask;
    (void)xCoreID;
    return pdPASS;
}

#ifdef __cplusplus
}
#endif

