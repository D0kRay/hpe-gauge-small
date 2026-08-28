#pragma once

#include "esp_err.h"
#include "lvgl.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *gauge;
} ui_screen_gauge_classic_t;

esp_err_t ui_screen_gauge_classic_init(ui_screen_gauge_classic_t *screen, lv_obj_t *parent, int speed_max);
void ui_screen_gauge_classic_set_value(ui_screen_gauge_classic_t *screen, int speed);
