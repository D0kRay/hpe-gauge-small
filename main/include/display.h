#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    bool pressed;
    int16_t x;
    int16_t y;
    int16_t dx;
    int16_t dy;
} display_touch_state_t;

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

/**
 * @brief Read the current touch state and deltas from the underlying CST816 device.
 */
esp_err_t display_get_touch_state(display_touch_state_t *touch);
