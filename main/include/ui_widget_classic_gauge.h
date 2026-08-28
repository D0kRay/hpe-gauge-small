#pragma once

#include "lvgl.h"

lv_obj_t *ui_widget_classic_gauge_create(lv_obj_t *parent, int speed_max);
void ui_widget_classic_gauge_set_value(lv_obj_t *widget, int speed);
