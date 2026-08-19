#pragma once

#include "esp_err.h"
#include "lvgl.h"

typedef void (*ui_screen_can_refresh_cb_t)(void *ctx);

typedef struct {
    lv_obj_t *root;
    lv_obj_t *panel;
    lv_obj_t *refresh_btn;
    ui_screen_can_refresh_cb_t refresh_cb;
    void *refresh_ctx;
} ui_screen_can_t;

esp_err_t ui_screen_can_init(ui_screen_can_t *screen, lv_obj_t *parent, ui_screen_can_refresh_cb_t refresh_cb, void *refresh_ctx);
void ui_screen_can_set_text(ui_screen_can_t *screen, const char *text);
