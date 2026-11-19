#pragma once

#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "driver/spi_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    spi_host_device_t host_id;
    gpio_num_t gpio_cs;
    gpio_num_t gpio_cd;
    gpio_num_t gpio_wp;
    gpio_num_t gpio_int;
} sdspi_device_config_t;

inline sdmmc_host_t SDSPI_HOST_DEFAULT(void) {
    sdmmc_host_t host{static_cast<int>(SPI2_HOST), 20000, 0u};
    return host;
}

inline sdspi_device_config_t SDSPI_DEVICE_CONFIG_DEFAULT(void) {
    sdspi_device_config_t cfg{SPI2_HOST, GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC};
    return cfg;
}

#ifdef __cplusplus
}
#endif
