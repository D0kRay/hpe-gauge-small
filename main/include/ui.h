#pragma once

#include <stdbool.h>
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

/**
 * @brief Update OTA status shown on the dedicated OTA screen.
 *
 * @param state_text Human-readable OTA state.
 * @param progress_percent OTA progress from 0..100.
 * @param success True once update is validated and ready for reboot.
 */
void ui_set_ota_status(const char *state_text, int progress_percent, bool success);
