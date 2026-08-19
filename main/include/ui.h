#pragma once

#include "esp_err.h"

esp_err_t ui_init(void);
void ui_set_speed_rpm(float speed_kmh, float rpm);
