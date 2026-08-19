#pragma once

#include "esp_err.h"

/**
 * @brief Register the `cancfg` console command set for CAN profile management.
 *
 * @return ESP_OK on success or an ESP_ERR_* code on registration failure.
 */
esp_err_t can_console_register(void);
