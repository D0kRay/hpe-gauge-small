#pragma once

#include "esp_err.h"

esp_err_t display_init(void);
void display_lvgl_lock(void);
void display_lvgl_unlock(void);
