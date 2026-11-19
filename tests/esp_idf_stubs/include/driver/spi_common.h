#pragma once

#include <cstdint>
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SPI1_HOST = 0,
    SPI2_HOST = 1,
    SPI3_HOST = 2
} spi_host_device_t;

typedef enum {
    SPI_DMA_DISABLED = 0,
    SPI_DMA_CH_AUTO  = 3
} spi_common_dma_t;

typedef struct {
    int mosi_io_num;
    int miso_io_num;
    int sclk_io_num;
    int quadwp_io_num;
    int quadhd_io_num;
    int max_transfer_sz;
} spi_bus_config_t;

inline esp_err_t spi_bus_initialize(spi_host_device_t host_id,
                                    const spi_bus_config_t* bus_config,
                                    spi_common_dma_t dma_chan) {
    if (bus_config == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)host_id;
    (void)dma_chan;
    return ESP_OK;
}

inline esp_err_t spi_bus_free(spi_host_device_t host_id) {
    (void)host_id;
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif
