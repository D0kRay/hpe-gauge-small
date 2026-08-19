#pragma once

#include "lvgl.h"

lv_obj_t *ui_widget_dual_gauge_create(lv_obj_t *parent, int speed_max, int rpm_max);
void ui_widget_dual_gauge_set_values(lv_obj_t *widget, int speed, int rpm);
