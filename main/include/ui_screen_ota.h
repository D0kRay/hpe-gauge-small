#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *panel;
} ui_screen_ota_t;

esp_err_t ui_screen_ota_init(ui_screen_ota_t *screen, lv_obj_t *parent);
void ui_screen_ota_set_status(ui_screen_ota_t *screen, const char *state_text, int progress_percent, bool success);
