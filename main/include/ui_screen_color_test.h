#pragma once

#include "esp_err.h"
#include "lvgl.h"

typedef struct {
    lv_obj_t *root;
} ui_screen_color_test_t;

typedef void (*ui_screen_color_test_done_cb_t)(void *ctx);

esp_err_t ui_screen_color_test_init(ui_screen_color_test_t *screen,
                                    lv_obj_t *parent,
                                    ui_screen_color_test_done_cb_t done_cb,
                                    void *done_ctx);
