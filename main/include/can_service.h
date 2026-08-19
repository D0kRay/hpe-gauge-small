#pragma once

#include "esp_err.h"

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
