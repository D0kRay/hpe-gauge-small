#include "ui.h"

#include <stdio.h>
#include "display.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "sdkconfig.h"

static lv_obj_t *s_speed_arc;
static lv_obj_t *s_rpm_arc;
static lv_obj_t *s_speed_label;
static lv_obj_t *s_rpm_label;
static lv_obj_t *s_menu_panel;

static void menu_btn_cb(lv_event_t *e)
{
    (void)e;
    if (lv_obj_has_flag(s_menu_panel, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(s_menu_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_menu_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

esp_err_t ui_init(void)
{
    display_lvgl_lock();

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);

    s_speed_arc = lv_arc_create(scr);
    lv_obj_set_size(s_speed_arc, 220, 220);
    lv_obj_center(s_speed_arc);
    lv_arc_set_rotation(s_speed_arc, 135);
    lv_arc_set_bg_angles(s_speed_arc, 0, 270);
    lv_arc_set_range(s_speed_arc, 0, CONFIG_HPE_UI_SPEED_MAX);
    lv_obj_remove_style(s_speed_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_speed_arc, LV_OBJ_FLAG_CLICKABLE);

    s_rpm_arc = lv_arc_create(scr);
    lv_obj_set_size(s_rpm_arc, 170, 170);
    lv_obj_center(s_rpm_arc);
    lv_arc_set_rotation(s_rpm_arc, 135);
    lv_arc_set_bg_angles(s_rpm_arc, 0, 270);
    lv_arc_set_range(s_rpm_arc, 0, CONFIG_HPE_UI_RPM_MAX);
    lv_obj_remove_style(s_rpm_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_rpm_arc, LV_OBJ_FLAG_CLICKABLE);

    s_speed_label = lv_label_create(scr);
    lv_label_set_text(s_speed_label, "0 km/h");
    lv_obj_align(s_speed_label, LV_ALIGN_CENTER, 0, -20);

    s_rpm_label = lv_label_create(scr);
    lv_label_set_text(s_rpm_label, "0 rpm");
    lv_obj_align(s_rpm_label, LV_ALIGN_CENTER, 0, 20);

    lv_obj_t *menu_btn = lv_button_create(scr);
    lv_obj_set_size(menu_btn, 64, 32);
    lv_obj_align(menu_btn, LV_ALIGN_TOP_RIGHT, -8, 8);
    lv_obj_add_event_cb(menu_btn, menu_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *menu_lbl = lv_label_create(menu_btn);
    lv_label_set_text(menu_lbl, "Menu");
    lv_obj_center(menu_lbl);

    s_menu_panel = lv_obj_create(scr);
    lv_obj_set_size(s_menu_panel, 130, 130);
    lv_obj_align(s_menu_panel, LV_ALIGN_BOTTOM_RIGHT, -8, -8);

    lv_obj_t *list = lv_list_create(s_menu_panel);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_list_add_text(list, "Quick Actions");
    lv_list_add_button(list, LV_SYMBOL_REFRESH, "Reload CAN cfg");
    lv_list_add_button(list, LV_SYMBOL_SETTINGS, "Calibration");
    lv_list_add_button(list, LV_SYMBOL_POWER, "Sleep");

    lv_obj_add_flag(s_menu_panel, LV_OBJ_FLAG_HIDDEN);

    display_lvgl_unlock();
    return ESP_OK;
}

void ui_set_speed_rpm(float speed_kmh, float rpm)
{
    if (!s_speed_arc || !s_rpm_arc) {
        return;
    }

    int speed = (int)speed_kmh;
    int rpm_i = (int)rpm;
    if (speed < 0) speed = 0;
    if (rpm_i < 0) rpm_i = 0;
    if (speed > CONFIG_HPE_UI_SPEED_MAX) speed = CONFIG_HPE_UI_SPEED_MAX;
    if (rpm_i > CONFIG_HPE_UI_RPM_MAX) rpm_i = CONFIG_HPE_UI_RPM_MAX;

    char speed_text[24];
    char rpm_text[24];
    snprintf(speed_text, sizeof(speed_text), "%d km/h", speed);
    snprintf(rpm_text, sizeof(rpm_text), "%d rpm", rpm_i);

    display_lvgl_lock();
    lv_arc_set_value(s_speed_arc, speed);
    lv_arc_set_value(s_rpm_arc, rpm_i);
    lv_label_set_text(s_speed_label, speed_text);
    lv_label_set_text(s_rpm_label, rpm_text);
    display_lvgl_unlock();
}
