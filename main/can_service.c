#include "can_service.h"

#include <string.h>
#include "bsp.h"
#include "can_cfg.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/twai_types.h"
#include "driver/twai.h"
#include "ui.h"

static const char *TAG = "can_service";

static float s_speed;
static float s_rpm;

static bool parse_signal(const can_signal_cfg_t *sig, const twai_message_t *msg, float *out)
{
    if (msg->identifier != sig->can_id) {
        return false;
    }
    if (sig->length_bytes == 0 || sig->length_bytes > 4 || sig->start_byte + sig->length_bytes > msg->data_length_code) {
        return false;
    }

    uint32_t raw = 0;
    for (uint8_t i = 0; i < sig->length_bytes; ++i) {
        raw |= ((uint32_t)msg->data[sig->start_byte + i]) << (8 * i);
    }
    *out = (raw * sig->scale) + sig->offset;
    return true;
}

static void can_rx_task(void *arg)
{
    (void)arg;
    can_signal_cfg_t speed_sig = {0};
    can_signal_cfg_t rpm_sig = {0};
    bool have_speed = can_cfg_get_signal("speed", &speed_sig);
    bool have_rpm = can_cfg_get_signal("rpm", &rpm_sig);

    while (true) {
        twai_message_t msg;
        if (twai_receive(&msg, pdMS_TO_TICKS(1000)) == ESP_OK) {
            if (have_speed) {
                parse_signal(&speed_sig, &msg, &s_speed);
            }
            if (have_rpm) {
                parse_signal(&rpm_sig, &msg, &s_rpm);
            }
            ui_set_speed_rpm(s_speed, s_rpm);
        }
    }
}

esp_err_t can_service_start(void)
{
    const bsp_config_t *bsp = bsp_config_get();

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(bsp->pin_twai_tx, bsp->pin_twai_rx, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();

    if (bsp->twai_bitrate == 250000) {
        t_config = TWAI_TIMING_CONFIG_250KBITS();
    } else if (bsp->twai_bitrate == 1000000) {
        t_config = TWAI_TIMING_CONFIG_1MBITS();
    }

    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_RETURN_ON_ERROR(twai_driver_install(&g_config, &t_config, &f_config), TAG, "twai install failed");
    ESP_RETURN_ON_ERROR(twai_start(), TAG, "twai start failed");

    ESP_RETURN_ON_FALSE(xTaskCreate(can_rx_task, "can_rx", 4096, NULL, 6, NULL) == pdPASS, ESP_ERR_NO_MEM, TAG, "can task create failed");

    return ESP_OK;
}
