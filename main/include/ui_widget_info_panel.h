#pragma once

#include "lvgl.h"

lv_obj_t *ui_widget_info_panel_create(lv_obj_t *parent);
void ui_widget_info_panel_set_title(lv_obj_t *widget, const char *title);
void ui_widget_info_panel_set_body(lv_obj_t *widget, const char *body);
