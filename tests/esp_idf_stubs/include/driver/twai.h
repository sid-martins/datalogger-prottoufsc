#pragma once

#include <cstdint>
#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TWAI_FRAME_MAX_DLC 8

#define TWAI_MSG_FLAG_NONE 0x00
#define TWAI_MSG_FLAG_EXTD 0x01
#define TWAI_MSG_FLAG_RTR  0x02

typedef enum {
    TWAI_MODE_NORMAL,
    TWAI_MODE_NO_ACK,
    TWAI_MODE_LISTEN_ONLY
} twai_mode_t;

typedef enum {
    TWAI_STATE_STOPPED,
    TWAI_STATE_RUNNING,
    TWAI_STATE_BUS_OFF,
    TWAI_STATE_RECOVERING
} twai_state_t;

typedef struct {
    twai_mode_t mode;
    gpio_num_t tx_io;
    gpio_num_t rx_io;
    gpio_num_t clkout_io;
    gpio_num_t bus_off_io;
    uint32_t tx_queue_len;
    uint32_t rx_queue_len;
    uint32_t alerts_enabled;
    uint32_t clkout_divider;
    int intr_flags;
} twai_general_config_t;

typedef struct {
    uint32_t brp;
    uint8_t tseg_1;
    uint8_t tseg_2;
    uint8_t sjw;
    bool triple_sampling;
} twai_timing_config_t;

typedef struct {
    uint32_t acceptance_code;
    uint32_t acceptance_mask;
    bool single_filter;
} twai_filter_config_t;

typedef struct {
    uint32_t flags;
    uint32_t identifier;
    uint8_t data_length_code;
    uint8_t data[TWAI_FRAME_MAX_DLC];
} twai_message_t;

typedef struct {
    twai_state_t state;
    uint32_t msgs_to_tx;
    uint32_t msgs_to_rx;
    uint32_t tx_error_counter;
    uint32_t rx_error_counter;
    uint32_t tx_failed_count;
    uint32_t rx_missed_count;
    uint32_t rx_overrun_count;
    uint32_t arb_lost_count;
    uint32_t bus_error_count;
} twai_status_info_t;

#define TWAI_GENERAL_CONFIG_DEFAULT(tx_gpio, rx_gpio, op_mode) \
    { (op_mode), (tx_gpio), (rx_gpio), GPIO_NUM_NC, GPIO_NUM_NC, 5, 5, 0, 0, 0 }

#define TWAI_TIMING_CONFIG_250KBITS() { 16, 15, 4, 3, false }
#define TWAI_TIMING_CONFIG_500KBITS() { 8, 15, 4, 3, false }
#define TWAI_TIMING_CONFIG_1MBITS()   { 4, 15, 4, 3, false }
#define TWAI_FILTER_CONFIG_ACCEPT_ALL() { 0, 0xFFFFFFFF, true }

inline esp_err_t twai_driver_install(const twai_general_config_t* g_config,
                                     const twai_timing_config_t* t_config,
                                     const twai_filter_config_t* f_config) {
    (void)g_config;
    (void)t_config;
    (void)f_config;
    return ESP_OK;
}

inline esp_err_t twai_driver_uninstall(void) { return ESP_OK; }
inline esp_err_t twai_start(void) { return ESP_OK; }
inline esp_err_t twai_stop(void) { return ESP_OK; }

inline esp_err_t twai_transmit(const twai_message_t* message, TickType_t ticks_to_wait) {
    (void)message;
    (void)ticks_to_wait;
    return ESP_OK;
}

inline esp_err_t twai_receive(twai_message_t* message, TickType_t ticks_to_wait) {
    (void)message;
    (void)ticks_to_wait;
    return ESP_OK;
}

inline esp_err_t twai_get_status_info(twai_status_info_t* status_info) {
    if (status_info != nullptr) {
        status_info->state = TWAI_STATE_RUNNING;
    }
    return ESP_OK;
}

inline esp_err_t twai_initiate_recovery(void) { return ESP_OK; }

#ifdef __cplusplus
}
#endif

