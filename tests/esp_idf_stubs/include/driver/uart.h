#pragma once

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#else
#include <stddef.h>
#include <stdint.h>
#endif

#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UART_PIN_NO_CHANGE (-1)
#define UART_FIFO_LEN      (128)

typedef enum {
    UART_NUM_0 = 0,
    UART_NUM_1 = 1,
    UART_NUM_2 = 2,
    UART_NUM_MAX
} uart_port_t;

typedef enum {
    UART_DATA_5_BITS   = 0x0,
    UART_DATA_6_BITS   = 0x1,
    UART_DATA_7_BITS   = 0x2,
    UART_DATA_8_BITS   = 0x3,
    UART_DATA_BITS_MAX = 0x4
} uart_word_length_t;

typedef enum {
    UART_PARITY_DISABLE = 0x0,
    UART_PARITY_EVEN    = 0x2,
    UART_PARITY_ODD     = 0x3
} uart_parity_t;

typedef enum {
    UART_STOP_BITS_1   = 0x1,
    UART_STOP_BITS_1_5 = 0x2,
    UART_STOP_BITS_2   = 0x3,
    UART_STOP_BITS_MAX = 0x4
} uart_stop_bits_t;

typedef enum {
    UART_HW_FLOWCTRL_DISABLE = 0x0,
    UART_HW_FLOWCTRL_RTS     = 0x1,
    UART_HW_FLOWCTRL_CTS     = 0x2,
    UART_HW_FLOWCTRL_CTS_RTS = 0x3,
    UART_HW_FLOWCTRL_MAX     = 0x4
} uart_hw_flowcontrol_t;

typedef enum {
    UART_SCLK_DEFAULT = 0,
    UART_SCLK_APB     = 0,
    UART_SCLK_XTAL    = 1,
    UART_SCLK_RTC     = 2
} uart_sclk_t;

typedef struct {
    int baud_rate;
    uart_word_length_t data_bits;
    uart_parity_t parity;
    uart_stop_bits_t stop_bits;
    uart_hw_flowcontrol_t flow_ctrl;
    uint8_t rx_flow_ctrl_thresh;
    union {
        uart_sclk_t source_clk;
    };
    struct {
        uint32_t allow_pd : 1;
        uint32_t backup_before_sleep : 1;
    } flags;
} uart_config_t;

#ifdef __cplusplus
#define UART_STUB_NULL   nullptr
#define UART_STUB_INT(x) static_cast<int>(x)
#else
#define UART_STUB_NULL   NULL
#define UART_STUB_INT(x) ((int)(x))
#endif

inline esp_err_t uart_driver_install(uart_port_t uart_num,
                                     int rx_buffer_size,
                                     int tx_buffer_size,
                                     int queue_size,
                                     QueueHandle_t* uart_queue,
                                     int intr_alloc_flags) {
    if (UART_STUB_INT(uart_num) < 0 || UART_STUB_INT(uart_num) >= UART_STUB_INT(UART_NUM_MAX) ||
        rx_buffer_size <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (uart_queue != UART_STUB_NULL) {
        *uart_queue = UART_STUB_NULL;
    }
    (void)tx_buffer_size;
    (void)queue_size;
    (void)intr_alloc_flags;
    return ESP_OK;
}

inline esp_err_t uart_driver_delete(uart_port_t uart_num) {
    if (UART_STUB_INT(uart_num) < 0 || UART_STUB_INT(uart_num) >= UART_STUB_INT(UART_NUM_MAX)) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

inline esp_err_t uart_param_config(uart_port_t uart_num, const uart_config_t* uart_config) {
    if (UART_STUB_INT(uart_num) < 0 || UART_STUB_INT(uart_num) >= UART_STUB_INT(UART_NUM_MAX) ||
        uart_config == UART_STUB_NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

inline esp_err_t uart_set_pin(uart_port_t uart_num,
                              int tx_io_num,
                              int rx_io_num,
                              int rts_io_num,
                              int cts_io_num) {
    if (UART_STUB_INT(uart_num) < 0 || UART_STUB_INT(uart_num) >= UART_STUB_INT(UART_NUM_MAX)) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)tx_io_num;
    (void)rx_io_num;
    (void)rts_io_num;
    (void)cts_io_num;
    return ESP_OK;
}

inline esp_err_t uart_set_baudrate(uart_port_t uart_num, uint32_t baudrate) {
    if (UART_STUB_INT(uart_num) < 0 || UART_STUB_INT(uart_num) >= UART_STUB_INT(UART_NUM_MAX) ||
        baudrate == 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

inline int uart_write_bytes(uart_port_t uart_num, const void* src, size_t size) {
    if (UART_STUB_INT(uart_num) < 0 || UART_STUB_INT(uart_num) >= UART_STUB_INT(UART_NUM_MAX) ||
        src == UART_STUB_NULL) {
        return -1;
    }
    return UART_STUB_INT(size);
}

inline esp_err_t uart_wait_tx_done(uart_port_t uart_num, TickType_t ticks_to_wait) {
    if (UART_STUB_INT(uart_num) < 0 || UART_STUB_INT(uart_num) >= UART_STUB_INT(UART_NUM_MAX)) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)ticks_to_wait;
    return ESP_OK;
}

inline int uart_read_bytes(uart_port_t uart_num,
                           void* buf,
                           uint32_t length,
                           TickType_t ticks_to_wait) {
    if (UART_STUB_INT(uart_num) < 0 || UART_STUB_INT(uart_num) >= UART_STUB_INT(UART_NUM_MAX) ||
        buf == UART_STUB_NULL) {
        return -1;
    }
    (void)length;
    (void)ticks_to_wait;
    return 0;
}

inline esp_err_t uart_flush_input(uart_port_t uart_num) {
    if (UART_STUB_INT(uart_num) < 0 || UART_STUB_INT(uart_num) >= UART_STUB_INT(UART_NUM_MAX)) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

#undef UART_STUB_INT
#undef UART_STUB_NULL

#ifdef __cplusplus
}
#endif
