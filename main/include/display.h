#pragma once

#include "esp_err.h"

/**
 * @brief Initialize LCD, touch input, LVGL buffers, and LVGL tick.
 *
 * @return ESP_OK on success or an ESP_ERR_* code on failure.
 */
esp_err_t display_init(void);

/**
 * @brief Start the LVGL task loop after UI objects are created.
 *
 * @return ESP_OK on success or an ESP_ERR_* code on failure.
 */
esp_err_t display_start(void);

/**
 * @brief Acquire the LVGL mutex for thread-safe UI updates.
 */
void display_lvgl_lock(void);

/**
 * @brief Release the LVGL mutex acquired by display_lvgl_lock().
 */
void display_lvgl_unlock(void);
