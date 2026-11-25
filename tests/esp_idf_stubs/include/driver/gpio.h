#pragma once

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GPIO_NUM_NC = -1,
    GPIO_NUM_0 = 0,
    GPIO_NUM_1 = 1,
    GPIO_NUM_2 = 2,
    GPIO_NUM_3 = 3,
    GPIO_NUM_4 = 4,
    GPIO_NUM_5 = 5,
    GPIO_NUM_6 = 6,
    GPIO_NUM_7 = 7,
    GPIO_NUM_8 = 8,
    GPIO_NUM_9 = 9,
    GPIO_NUM_10 = 10,
    GPIO_NUM_11 = 11,
    GPIO_NUM_12 = 12,
    GPIO_NUM_13 = 13,
    GPIO_NUM_14 = 14,
    GPIO_NUM_15 = 15,
    GPIO_NUM_16 = 16,
    GPIO_NUM_17 = 17,
    GPIO_NUM_18 = 18,
    GPIO_NUM_19 = 19,
    GPIO_NUM_20 = 20,
    GPIO_NUM_21 = 21,
    GPIO_NUM_26 = 26,
    GPIO_NUM_27 = 27,
    GPIO_NUM_28 = 28,
    GPIO_NUM_29 = 29,
    GPIO_NUM_30 = 30,
    GPIO_NUM_31 = 31,
    GPIO_NUM_32 = 32,
    GPIO_NUM_33 = 33,
    GPIO_NUM_34 = 34,
    GPIO_NUM_35 = 35,
    GPIO_NUM_36 = 36,
    GPIO_NUM_37 = 37,
    GPIO_NUM_38 = 38,
    GPIO_NUM_39 = 39,
    GPIO_NUM_40 = 40,
    GPIO_NUM_41 = 41,
    GPIO_NUM_42 = 42,
    GPIO_NUM_43 = 43,
    GPIO_NUM_44 = 44,
    GPIO_NUM_45 = 45,
    GPIO_NUM_46 = 46,
    GPIO_NUM_47 = 47,
    GPIO_NUM_48 = 48,
    GPIO_NUM_MAX
} gpio_num_t;

typedef enum {
    GPIO_INTR_DISABLE = 0,
    GPIO_INTR_POSEDGE = 1,
    GPIO_INTR_NEGEDGE = 2,
    GPIO_INTR_ANYEDGE = 3,
    GPIO_INTR_LOW_LEVEL = 4,
    GPIO_INTR_HIGH_LEVEL = 5,
    GPIO_INTR_MAX
} gpio_int_type_t;

typedef enum {
    GPIO_MODE_DISABLE = 0,
    GPIO_MODE_INPUT = 1,
    GPIO_MODE_OUTPUT = 2,
    GPIO_MODE_INPUT_OUTPUT = 3,
    GPIO_MODE_OUTPUT_OD = 6,
    GPIO_MODE_INPUT_OUTPUT_OD = 7
} gpio_mode_t;

typedef enum {
    GPIO_PULLUP_DISABLE = 0x0,
    GPIO_PULLUP_ENABLE = 0x1
} gpio_pullup_t;

typedef enum {
    GPIO_PULLDOWN_DISABLE = 0x0,
    GPIO_PULLDOWN_ENABLE = 0x1
} gpio_pulldown_t;

typedef struct {
    uint64_t pin_bit_mask;
    gpio_mode_t mode;
    gpio_pullup_t pull_up_en;
    gpio_pulldown_t pull_down_en;
    gpio_int_type_t intr_type;
} gpio_config_t;

#ifdef __cplusplus
#define GPIO_STUB_NULL nullptr
#define GPIO_STUB_INT(x) static_cast<int>(x)
#else
#define GPIO_STUB_NULL ((void*)0)
#define GPIO_STUB_INT(x) ((int)(x))
#endif

inline esp_err_t gpio_config(const gpio_config_t* pGPIOConfig) {
    if (pGPIOConfig == GPIO_STUB_NULL || pGPIOConfig->pin_bit_mask == 0ULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

inline esp_err_t gpio_set_level(gpio_num_t gpio_num, uint32_t level) {
    if (GPIO_STUB_INT(gpio_num) < 0 || GPIO_STUB_INT(gpio_num) >= GPIO_STUB_INT(GPIO_NUM_MAX)) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)level;
    return ESP_OK;
}

inline int gpio_get_level(gpio_num_t gpio_num) {
    if (GPIO_STUB_INT(gpio_num) < 0 || GPIO_STUB_INT(gpio_num) >= GPIO_STUB_INT(GPIO_NUM_MAX)) {
        return 0;
    }
    return 1;
}

inline esp_err_t gpio_reset_pin(gpio_num_t gpio_num) {
    if (GPIO_STUB_INT(gpio_num) < 0 || GPIO_STUB_INT(gpio_num) >= GPIO_STUB_INT(GPIO_NUM_MAX)) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

#undef GPIO_STUB_INT
#undef GPIO_STUB_NULL

#ifdef __cplusplus
}
#endif
