#pragma once

#include "esp_err.h"
#include "lvgl.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *dual_gauge;
} ui_screen_gauge_t;

esp_err_t ui_screen_gauge_init(ui_screen_gauge_t *screen, lv_obj_t *parent, int speed_max, int rpm_max);
void ui_screen_gauge_set_values(ui_screen_gauge_t *screen, int speed, int rpm);
