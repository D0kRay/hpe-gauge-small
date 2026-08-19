#include "ui_widget_dual_gauge.h"

#include <stdio.h>
#include "esp_check.h"

typedef struct {
    lv_obj_t obj;
    lv_obj_t *speed_arc;
    lv_obj_t *rpm_arc;
    lv_obj_t *speed_label;
    lv_obj_t *rpm_label;
    int speed_max;
    int rpm_max;
} ui_widget_dual_gauge_t;

static void dual_gauge_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    ui_widget_dual_gauge_t *w = (ui_widget_dual_gauge_t *)obj;

    lv_obj_set_size(obj, 220, 220);

    w->speed_arc = lv_arc_create(obj);
    lv_obj_set_size(w->speed_arc, 220, 220);
    lv_obj_center(w->speed_arc);
    lv_arc_set_rotation(w->speed_arc, 135);
    lv_arc_set_bg_angles(w->speed_arc, 0, 270);
    lv_arc_set_range(w->speed_arc, 0, 240);
    lv_obj_remove_style(w->speed_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(w->speed_arc, LV_OBJ_FLAG_CLICKABLE);

    w->rpm_arc = lv_arc_create(obj);
    lv_obj_set_size(w->rpm_arc, 170, 170);
    lv_obj_center(w->rpm_arc);
    lv_arc_set_rotation(w->rpm_arc, 135);
    lv_arc_set_bg_angles(w->rpm_arc, 0, 270);
    lv_arc_set_range(w->rpm_arc, 0, 9000);
    lv_obj_remove_style(w->rpm_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(w->rpm_arc, LV_OBJ_FLAG_CLICKABLE);

    w->speed_label = lv_label_create(obj);
    lv_label_set_text(w->speed_label, "0 km/h");
    lv_obj_align(w->speed_label, LV_ALIGN_CENTER, 0, -20);

    w->rpm_label = lv_label_create(obj);
    lv_label_set_text(w->rpm_label, "0 rpm");
    lv_obj_align(w->rpm_label, LV_ALIGN_CENTER, 0, 20);

    w->speed_max = 240;
    w->rpm_max = 9000;
}

static const lv_obj_class_t s_dual_gauge_class = {
    .constructor_cb = dual_gauge_constructor,
    .instance_size = sizeof(ui_widget_dual_gauge_t),
    .base_class = &lv_obj_class,
    .name = "ui_dual_gauge",
};

lv_obj_t *ui_widget_dual_gauge_create(lv_obj_t *parent, int speed_max, int rpm_max)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&s_dual_gauge_class, parent);
    lv_obj_class_init_obj(obj);

    ui_widget_dual_gauge_t *w = (ui_widget_dual_gauge_t *)obj;
    w->speed_max = (speed_max > 0) ? speed_max : 1;
    w->rpm_max = (rpm_max > 0) ? rpm_max : 1;
    lv_arc_set_range(w->speed_arc, 0, w->speed_max);
    lv_arc_set_range(w->rpm_arc, 0, w->rpm_max);
    return obj;
}

void ui_widget_dual_gauge_set_values(lv_obj_t *widget, int speed, int rpm)
{
    if (!widget) {
        return;
    }

    ui_widget_dual_gauge_t *w = (ui_widget_dual_gauge_t *)widget;
    if (speed < 0) {
        speed = 0;
    }
    if (rpm < 0) {
        rpm = 0;
    }
    if (speed > w->speed_max) {
        speed = w->speed_max;
    }
    if (rpm > w->rpm_max) {
        rpm = w->rpm_max;
    }

    char speed_text[24];
    char rpm_text[24];
    snprintf(speed_text, sizeof(speed_text), "%d km/h", speed);
    snprintf(rpm_text, sizeof(rpm_text), "%d rpm", rpm);

    lv_arc_set_value(w->speed_arc, speed);
    lv_arc_set_value(w->rpm_arc, rpm);
    lv_label_set_text(w->speed_label, speed_text);
    lv_label_set_text(w->rpm_label, rpm_text);
}
