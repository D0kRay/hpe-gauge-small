#include "can_service.h"

#include <string.h>
#include "bsp.h"
#include "can_cfg.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/twai_types.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#include "driver/twai.h"
#pragma GCC diagnostic pop
#include "ui.h"

static const char *TAG = "can_service";

static float s_speed;
static float s_rpm;
static float s_signal_values[CAN_CFG_MAX_SIGNALS];
static bool s_signal_has_value[CAN_CFG_MAX_SIGNALS];
static portMUX_TYPE s_data_lock = portMUX_INITIALIZER_UNLOCKED;

static bool get_bit_lsb_indexed(const uint8_t *data, uint8_t dlc, int bit_index, uint8_t *out_bit)
{
    if (bit_index < 0 || bit_index >= (dlc * 8)) {
        return false;
    }
    uint8_t byte_index = (uint8_t)(bit_index / 8);
    uint8_t bit_in_byte = (uint8_t)(bit_index % 8);
    *out_bit = (data[byte_index] >> bit_in_byte) & 0x01U;
    return true;
}

static int motorola_next_bit(int bit_index)
{
    return (bit_index % 8 == 0) ? (bit_index + 15) : (bit_index - 1);
}

static bool extract_raw_dbc(const can_signal_cfg_t *sig, const twai_message_t *msg, uint32_t *raw_out)
{
    if (sig->bit_length == 0 || sig->bit_length > 32) {
        return false;
    }
    if (sig->start_bit >= (msg->data_length_code * 8)) {
        return false;
    }

    uint32_t raw = 0;
    if (sig->is_little_endian) {
        uint64_t frame = 0;
        for (uint8_t i = 0; i < msg->data_length_code; ++i) {
            frame |= ((uint64_t)msg->data[i]) << (8 * i);
        }
        if ((sig->start_bit + sig->bit_length) > (msg->data_length_code * 8)) {
            return false;
        }
        uint64_t mask = (sig->bit_length == 32) ? 0xFFFFFFFFULL : ((1ULL << sig->bit_length) - 1ULL);
        raw = (uint32_t)((frame >> sig->start_bit) & mask);
    } else {
        int bit_idx = sig->start_bit;
        for (uint8_t i = 0; i < sig->bit_length; ++i) {
            uint8_t bit = 0;
            if (!get_bit_lsb_indexed(msg->data, msg->data_length_code, bit_idx, &bit)) {
                return false;
            }
            raw = (raw << 1) | bit;
            bit_idx = motorola_next_bit(bit_idx);
        }
    }

    *raw_out = raw;
    return true;
}

static bool parse_signal(const can_signal_cfg_t *sig, const twai_message_t *msg, float *out)
{
    if (msg->identifier != sig->can_id) {
        return false;
    }
    uint32_t raw = 0;
    if (!extract_raw_dbc(sig, msg, &raw)) {
        return false;
    }
    int32_t signed_raw = (int32_t)raw;
    if (sig->is_signed && sig->bit_length < 32) {
        uint32_t sign_bit = 1U << (sig->bit_length - 1);
        if ((raw & sign_bit) != 0) {
            signed_raw = (int32_t)(raw | (~0U << sig->bit_length));
        }
    }

    *out = ((sig->is_signed ? (float)signed_raw : (float)raw) * sig->factor) + sig->offset;
    return true;
}

static void can_rx_task(void *arg)
{
    (void)arg;
    while (true) {
        twai_message_t msg;
        if (twai_receive(&msg, pdMS_TO_TICKS(1000)) == ESP_OK) {
            const can_cfg_t *cfg = can_cfg_get();
            float speed = 0.0f;
            float rpm = 0.0f;
            float signal_values[CAN_CFG_MAX_SIGNALS];
            bool signal_has_value[CAN_CFG_MAX_SIGNALS];

            portENTER_CRITICAL(&s_data_lock);
            speed = s_speed;
            rpm = s_rpm;
            memcpy(signal_values, s_signal_values, sizeof(signal_values));
            memcpy(signal_has_value, s_signal_has_value, sizeof(signal_has_value));
            portEXIT_CRITICAL(&s_data_lock);

            for (uint8_t i = 0; i < cfg->signal_count && i < CAN_CFG_MAX_SIGNALS; ++i) {
                float parsed_value = 0.0f;
                if (!parse_signal(&cfg->signals[i], &msg, &parsed_value)) {
                    continue;
                }

                signal_values[i] = parsed_value;
                signal_has_value[i] = true;

                if (strncmp(cfg->signals[i].name, "speed", CAN_SIGNAL_NAME_MAX) == 0) {
                    speed = parsed_value;
                } else if (strncmp(cfg->signals[i].name, "rpm", CAN_SIGNAL_NAME_MAX) == 0) {
                    rpm = parsed_value;
                }
            }

            portENTER_CRITICAL(&s_data_lock);
            s_speed = speed;
            s_rpm = rpm;
            memcpy(s_signal_values, signal_values, sizeof(s_signal_values));
            memcpy(s_signal_has_value, signal_has_value, sizeof(s_signal_has_value));
            portEXIT_CRITICAL(&s_data_lock);
            ui_set_speed_rpm(speed, rpm);
        }
    }
}

esp_err_t can_service_start(void)
{
    const bsp_config_t *bsp = bsp_config_get();

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(bsp->pin_twai_tx, bsp->pin_twai_rx, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();

    if (bsp->twai_bitrate == 250000) {
        t_config = (twai_timing_config_t)TWAI_TIMING_CONFIG_250KBITS();
    } else if (bsp->twai_bitrate == 1000000) {
        t_config = (twai_timing_config_t)TWAI_TIMING_CONFIG_1MBITS();
    }

    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_RETURN_ON_ERROR(twai_driver_install(&g_config, &t_config, &f_config), TAG, "twai install failed");
    ESP_RETURN_ON_ERROR(twai_start(), TAG, "twai start failed");

    ESP_RETURN_ON_FALSE(xTaskCreate(can_rx_task, "can_rx", 4096, NULL, 6, NULL) == pdPASS, ESP_ERR_NO_MEM, TAG, "can task create failed");

    return ESP_OK;
}

void can_service_get_values(float *out_speed_kmh, float *out_rpm)
{
    portENTER_CRITICAL(&s_data_lock);
    float speed = s_speed;
    float rpm = s_rpm;
    portEXIT_CRITICAL(&s_data_lock);

    if (out_speed_kmh) {
        *out_speed_kmh = speed;
    }
    if (out_rpm) {
        *out_rpm = rpm;
    }
}

size_t can_service_get_signal_values(can_service_signal_value_t *out_values, size_t max_values)
{
    const can_cfg_t *cfg = can_cfg_get();
    size_t signal_count = cfg->signal_count;
    if (signal_count > CAN_CFG_MAX_SIGNALS) {
        signal_count = CAN_CFG_MAX_SIGNALS;
    }

    if (!out_values || max_values == 0) {
        return signal_count;
    }

    size_t to_copy = signal_count < max_values ? signal_count : max_values;
    portENTER_CRITICAL(&s_data_lock);
    for (size_t i = 0; i < to_copy; ++i) {
        memset(&out_values[i], 0, sizeof(out_values[i]));
        strncpy(out_values[i].name, cfg->signals[i].name, CAN_SIGNAL_NAME_MAX - 1);
        out_values[i].value = s_signal_values[i];
        out_values[i].has_value = s_signal_has_value[i];
    }
    portEXIT_CRITICAL(&s_data_lock);

    return signal_count;
}
