#pragma once

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t TickType_t;

#define pdTRUE  ((BaseType_t)1)
#define pdFALSE ((BaseType_t)0)
#define pdPASS  (pdTRUE)
#define pdFAIL  (pdFALSE)

#define configMAX_PRIORITIES (25)
#define tskIDLE_PRIORITY     ((UBaseType_t)0U)

#define pdMS_TO_TICKS(xTimeInMs) ((TickType_t)(xTimeInMs))

#ifdef __cplusplus
}
#endif

