#pragma once

#include "esp_err.h"

/**
 * @brief Initialize TWAI and start the CAN receive task that feeds UI values.
 *
 * @return ESP_OK on success or an ESP_ERR_* code on driver/task failures.
 */
esp_err_t can_service_start(void);
