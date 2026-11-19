#pragma once

#include <cstdint>
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDMMC_SLOT_FLAG_INTERNAL_PULLUP (1U << 0)

typedef struct {
    int slot;
    int max_freq_khz;
    uint32_t flags;
} sdmmc_host_t;

typedef struct {
    gpio_num_t clk;
    gpio_num_t cmd;
    gpio_num_t d0;
    gpio_num_t d1;
    gpio_num_t d2;
    gpio_num_t d3;
    gpio_num_t cd;
    gpio_num_t wp;
    uint8_t width;
    uint32_t flags;
} sdmmc_slot_config_t;

inline sdmmc_host_t SDMMC_HOST_DEFAULT(void) {
    sdmmc_host_t host{1, 20000, 0u};
    return host;
}

inline sdmmc_slot_config_t SDMMC_SLOT_CONFIG_DEFAULT(void) {
    sdmmc_slot_config_t cfg{
        GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC,
        GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC,
        4u, 0u
    };
    return cfg;
}

#ifdef __cplusplus
}
#endif
