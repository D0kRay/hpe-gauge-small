#pragma once

#include "esp_err.h"
#include "lvgl.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *panel;
} ui_screen_sensors_t;

esp_err_t ui_screen_sensors_init(ui_screen_sensors_t *screen, lv_obj_t *parent);
void ui_screen_sensors_set_text(ui_screen_sensors_t *screen, const char *text);
