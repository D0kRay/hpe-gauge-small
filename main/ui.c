#include "ui.h"

#include <stdio.h>
#include "can_cfg.h"
#include "display.h"
#include "sensor_rtc.h"
#include "lvgl.h"
#include "sdkconfig.h"

static lv_obj_t *s_speed_arc;
static lv_obj_t *s_rpm_arc;
static lv_obj_t *s_speed_label;
static lv_obj_t *s_rpm_label;
static lv_obj_t *s_can_label;
static lv_obj_t *s_sensor_label;

static void ui_refresh_pages(void)
{
    if (s_can_label) {
        const can_cfg_t *cfg = can_cfg_get();
        char can_text[420];
        int off = snprintf(can_text, sizeof(can_text), "CAN profile (%u signals)\\n", cfg->signal_count);
        if (off < 0) {
            off = 0;
            can_text[0] = '\0';
        }
        for (uint8_t i = 0; i < cfg->signal_count && off < (int)sizeof(can_text) - 1; ++i) {
            const can_signal_cfg_t *s = &cfg->signals[i];
            size_t remaining = sizeof(can_text) - (size_t)off;
            int wrote = snprintf(&can_text[off], remaining,
                                 "%s id:%03lx b:%u len:%u %s s:%u f:%.3f o:%.2f\\n",
                                 s->name,
                                 (unsigned long)s->can_id,
                                 s->start_bit,
                                 s->bit_length,
                                 s->is_little_endian ? "le" : "be",
                                 s->is_signed ? 1U : 0U,
                                 (double)s->factor,
                                 (double)s->offset);
            if (wrote < 0) {
                break;
            }
            if (wrote >= (int)remaining) {
                off = (int)sizeof(can_text) - 1;
                can_text[off] = '\0';
                break;
            }
            off += wrote;
        }
        lv_label_set_text(s_can_label, can_text);
    }

    if (s_sensor_label) {
        sensor_rtc_status_t st = sensor_rtc_get_status();
        char sensor_text[160];
        snprintf(sensor_text, sizeof(sensor_text),
                 "IMU QMI8658: %s (WHOAMI 0x%02X)\\nRTC PCF85063: %s (CTRL1 0x%02X)",
                 st.imu_detected ? "detected" : "not found",
                 st.imu_whoami,
                 st.rtc_detected ? "detected" : "not found",
                 st.rtc_ctrl1);
        lv_label_set_text(s_sensor_label, sensor_text);
    }
}

static void ui_refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_refresh_pages();
}

static void can_refresh_btn_cb(lv_event_t *e)
{
    (void)e;
    ui_refresh_pages();
}

esp_err_t ui_init(void)
{
    display_lvgl_lock();

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);

    lv_obj_t *tv = lv_tabview_create(scr);
    lv_obj_set_size(tv, lv_pct(100), lv_pct(100));

    lv_obj_t *tab_gauge = lv_tabview_add_tab(tv, "Gauge");
    lv_obj_t *tab_can = lv_tabview_add_tab(tv, "CAN");
    lv_obj_t *tab_sensors = lv_tabview_add_tab(tv, "Sensors");

    s_speed_arc = lv_arc_create(tab_gauge);
    lv_obj_set_size(s_speed_arc, 220, 220);
    lv_obj_center(s_speed_arc);
    lv_arc_set_rotation(s_speed_arc, 135);
    lv_arc_set_bg_angles(s_speed_arc, 0, 270);
    lv_arc_set_range(s_speed_arc, 0, CONFIG_HPE_UI_SPEED_MAX);
    lv_obj_remove_style(s_speed_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_speed_arc, LV_OBJ_FLAG_CLICKABLE);

    s_rpm_arc = lv_arc_create(tab_gauge);
    lv_obj_set_size(s_rpm_arc, 170, 170);
    lv_obj_center(s_rpm_arc);
    lv_arc_set_rotation(s_rpm_arc, 135);
    lv_arc_set_bg_angles(s_rpm_arc, 0, 270);
    lv_arc_set_range(s_rpm_arc, 0, CONFIG_HPE_UI_RPM_MAX);
    lv_obj_remove_style(s_rpm_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_rpm_arc, LV_OBJ_FLAG_CLICKABLE);

    s_speed_label = lv_label_create(tab_gauge);
    lv_label_set_text(s_speed_label, "0 km/h");
    lv_obj_align(s_speed_label, LV_ALIGN_CENTER, 0, -20);

    s_rpm_label = lv_label_create(tab_gauge);
    lv_label_set_text(s_rpm_label, "0 rpm");
    lv_obj_align(s_rpm_label, LV_ALIGN_CENTER, 0, 20);

    s_can_label = lv_label_create(tab_can);
    lv_obj_set_width(s_can_label, lv_pct(100));
    lv_label_set_long_mode(s_can_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_can_label, LV_ALIGN_TOP_LEFT, 4, 4);

    lv_obj_t *can_refresh = lv_button_create(tab_can);
    lv_obj_set_size(can_refresh, 94, 32);
    lv_obj_align(can_refresh, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
    lv_obj_add_event_cb(can_refresh, can_refresh_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *can_refresh_lbl = lv_label_create(can_refresh);
    lv_label_set_text(can_refresh_lbl, "Refresh");
    lv_obj_center(can_refresh_lbl);

    s_sensor_label = lv_label_create(tab_sensors);
    lv_obj_set_width(s_sensor_label, lv_pct(100));
    lv_label_set_long_mode(s_sensor_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_sensor_label, LV_ALIGN_TOP_LEFT, 4, 4);

    ui_refresh_pages();
    lv_timer_create(ui_refresh_timer_cb, 1000, NULL);

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
