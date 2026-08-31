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

/**
 * @brief Switch the active UI screen by name.
 *
 * Supported names: gauge, classic, color, can, sensors, ota, raycaster.
 */
esp_err_t ui_switch_screen_name(const char *screen_name);

/**
 * @brief Dump a compact ASCII preview of the current raycaster framebuffer.
 *
 * @param use_render_buffer If true, dump the working render buffer; otherwise dump
 * the canvas buffer currently displayed to LVGL.
 */
void ui_debug_dump_raycaster_frame(bool use_render_buffer);
