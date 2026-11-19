#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ADC_UNIT_1 = 0,
    ADC_UNIT_2 = 1
} adc_unit_t;

typedef enum {
    ADC_CHANNEL_0 = 0,
    ADC_CHANNEL_1,
    ADC_CHANNEL_2,
    ADC_CHANNEL_3,
    ADC_CHANNEL_4,
    ADC_CHANNEL_5,
    ADC_CHANNEL_6,
    ADC_CHANNEL_7,
    ADC_CHANNEL_8,
    ADC_CHANNEL_9
} adc_channel_t;

typedef enum {
    ADC_BITWIDTH_DEFAULT = 0,
    ADC_BITWIDTH_9       = 9,
    ADC_BITWIDTH_10      = 10,
    ADC_BITWIDTH_11      = 11,
    ADC_BITWIDTH_12      = 12
} adc_bitwidth_t;

typedef enum {
    ADC_ATTEN_DB_0   = 0,
    ADC_ATTEN_DB_2_5 = 1,
    ADC_ATTEN_DB_6   = 2,
    ADC_ATTEN_DB_12  = 3
} adc_atten_t;

typedef enum {
    ADC_ULP_MODE_DISABLE = 0
} adc_ulp_mode_t;

typedef struct adc_oneshot_unit_ctx_t* adc_oneshot_unit_handle_t;

typedef struct {
    adc_unit_t unit_id;
    adc_ulp_mode_t ulp_mode;
} adc_oneshot_unit_init_cfg_t;

typedef struct {
    adc_atten_t atten;
    adc_bitwidth_t bitwidth;
} adc_oneshot_chan_cfg_t;

inline esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t* init_config,
                                      adc_oneshot_unit_handle_t* ret_unit) {
    if (init_config == nullptr || ret_unit == nullptr) {
        return ESP_FAIL;
    }
#ifdef __cplusplus
    *ret_unit = reinterpret_cast<adc_oneshot_unit_handle_t>(1);
#else
    *ret_unit = (adc_oneshot_unit_handle_t)(1);
#endif
    return ESP_OK;
}

inline esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t handle,
                                            adc_channel_t channel,
                                            const adc_oneshot_chan_cfg_t* config) {
    if (handle == nullptr || config == nullptr) {
        return ESP_FAIL;
    }
    (void)channel;
    return ESP_OK;
}

inline esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t handle,
                                  adc_channel_t chan,
                                  int* out_raw) {
    if (handle == nullptr || out_raw == nullptr) {
        return ESP_FAIL;
    }
    (void)chan;
    *out_raw = 0;
    return ESP_OK;
}

inline esp_err_t adc_oneshot_del_unit(adc_oneshot_unit_handle_t handle) {
    (void)handle;
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif
