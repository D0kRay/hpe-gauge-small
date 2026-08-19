#pragma once

#include "esp_err.h"

/**
 * @brief Start WiFi access point and HTTP/WebSocket web UI server.
 *
 * @return ESP_OK on success or an ESP_ERR_* code on initialization failure.
 */
esp_err_t web_ui_start(void);
