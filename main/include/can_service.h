#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "can_cfg.h"

/**
 * @brief Initialize TWAI and start the CAN receive task that feeds UI values.
 *
 * @return ESP_OK on success or an ESP_ERR_* code on driver/task failures.
 */
esp_err_t can_service_start(void);

/**
 * @brief Get latest decoded speed and RPM values.
 *
 * @param out_speed_kmh Output pointer for speed in km/h (optional).
 * @param out_rpm Output pointer for rpm value (optional).
 */
void can_service_get_values(float *out_speed_kmh, float *out_rpm);

/**
 * @brief Latest decoded value for one configured CAN signal.
 */
typedef struct {
    char name[CAN_SIGNAL_NAME_MAX];
    float value;
    bool has_value;
} can_service_signal_value_t;

/**
 * @brief Copy latest decoded values for all configured CAN signals.
 *
 * @param out_values Destination array (optional).
 * @param max_values Capacity of out_values.
 * @return Number of configured signals copied/available.
 */
size_t can_service_get_signal_values(can_service_signal_value_t *out_values, size_t max_values);
