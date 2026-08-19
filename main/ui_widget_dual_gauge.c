#include "ui_widget_dual_gauge.h"

#include <stdio.h>

static lv_obj_t *dual_gauge_speed_arc_obj(lv_obj_t *widget)
{
    return widget ? lv_obj_get_child(widget, 0) : NULL;
}

static lv_obj_t *dual_gauge_rpm_arc_obj(lv_obj_t *widget)
{
    return widget ? lv_obj_get_child(widget, 1) : NULL;
}

static lv_obj_t *dual_gauge_speed_label_obj(lv_obj_t *widget)
{
    return widget ? lv_obj_get_child(widget, 2) : NULL;
}

static lv_obj_t *dual_gauge_rpm_label_obj(lv_obj_t *widget)
{
    return widget ? lv_obj_get_child(widget, 3) : NULL;
}

lv_obj_t *ui_widget_dual_gauge_create(lv_obj_t *parent, int speed_max, int rpm_max)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, 220, 220);

    lv_obj_t *speed_arc = lv_arc_create(obj);
    lv_obj_set_size(speed_arc, 220, 220);
    lv_obj_center(speed_arc);
    lv_arc_set_rotation(speed_arc, 135);
    lv_arc_set_bg_angles(speed_arc, 0, 270);
    lv_arc_set_range(speed_arc, 0, (speed_max > 0) ? speed_max : 1);
    lv_obj_remove_style(speed_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(speed_arc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *rpm_arc = lv_arc_create(obj);
    lv_obj_set_size(rpm_arc, 170, 170);
    lv_obj_center(rpm_arc);
    lv_arc_set_rotation(rpm_arc, 135);
    lv_arc_set_bg_angles(rpm_arc, 0, 270);
    lv_arc_set_range(rpm_arc, 0, (rpm_max > 0) ? rpm_max : 1);
    lv_obj_remove_style(rpm_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(rpm_arc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *speed_label = lv_label_create(obj);
    lv_label_set_text(speed_label, "0 km/h");
    lv_obj_align(speed_label, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *rpm_label = lv_label_create(obj);
    lv_label_set_text(rpm_label, "0 rpm");
    lv_obj_align(rpm_label, LV_ALIGN_CENTER, 0, 20);

    return obj;
}

void ui_widget_dual_gauge_set_values(lv_obj_t *widget, int speed, int rpm)
{
    if (!widget) {
        return;
    }

    lv_obj_t *speed_arc = dual_gauge_speed_arc_obj(widget);
    lv_obj_t *rpm_arc = dual_gauge_rpm_arc_obj(widget);
    lv_obj_t *speed_label = dual_gauge_speed_label_obj(widget);
    lv_obj_t *rpm_label = dual_gauge_rpm_label_obj(widget);
    if (!speed_arc || !rpm_arc || !speed_label || !rpm_label) {
        return;
    }

    int speed_max = lv_arc_get_max_value(speed_arc);
    int rpm_max = lv_arc_get_max_value(rpm_arc);
    if (speed < 0) {
        speed = 0;
    }
    if (rpm < 0) {
        rpm = 0;
    }
    if (speed > speed_max) {
        speed = speed_max;
    }
    if (rpm > rpm_max) {
        rpm = rpm_max;
    }

    char speed_text[24];
    char rpm_text[24];
    snprintf(speed_text, sizeof(speed_text), "%d km/h", speed);
    snprintf(rpm_text, sizeof(rpm_text), "%d rpm", rpm);

    lv_arc_set_value(speed_arc, speed);
    lv_arc_set_value(rpm_arc, rpm);
    lv_label_set_text(speed_label, speed_text);
    lv_label_set_text(rpm_label, rpm_text);
}
