#pragma once

#include "esp_err.h"

/**
 * @brief Create and initialize the LVGL tabs, widgets, and periodic refresh timer.
 *
 * @return ESP_OK on success or an ESP_ERR_* code on failure.
 */
esp_err_t ui_init(void);

/**
 * @brief Update gauge values shown on the UI.
 *
 * @param speed_kmh Vehicle speed in km/h.
 * @param rpm Engine speed in RPM.
 */
void ui_set_speed_rpm(float speed_kmh, float rpm);
