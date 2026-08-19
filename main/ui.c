#include "ui.h"

#include <stdio.h>
#include "can_cfg.h"
#include "display.h"
#include "lvgl.h"
#include "sdkconfig.h"
#include "sensor_rtc.h"
#include "ui_screen_can.h"
#include "ui_screen_gauge.h"
#include "ui_screen_ota.h"
#include "ui_screen_sensors.h"

static ui_screen_gauge_t s_gauge_screen;
static ui_screen_can_t s_can_screen;
static ui_screen_sensors_t s_sensors_screen;
static ui_screen_ota_t s_ota_screen;

static void ui_build_can_text(char *out, size_t out_size)
{
    if (!out || out_size == 0) {
        return;
    }

    const can_cfg_t *cfg = can_cfg_get();
    int off = snprintf(out, out_size, "CAN profile (%u signals)\\n", cfg->signal_count);
    if (off < 0) {
        out[0] = '\0';
        return;
    }

    for (uint8_t i = 0; i < cfg->signal_count && off < (int)out_size - 1; ++i) {
        const can_signal_cfg_t *s = &cfg->signals[i];
        size_t remaining = out_size - (size_t)off;
        int wrote = snprintf(&out[off], remaining,
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
            off = (int)out_size - 1;
            out[off] = '\0';
            break;
        }
        off += wrote;
    }
}

static void ui_build_sensor_text(char *out, size_t out_size)
{
    if (!out || out_size == 0) {
        return;
    }

    sensor_rtc_status_t st = sensor_rtc_get_status();
    snprintf(out, out_size,
             "IMU QMI8658: %s (WHOAMI 0x%02X)\\nRTC PCF85063: %s (CTRL1 0x%02X)",
             st.imu_detected ? "detected" : "not found",
             st.imu_whoami,
             st.rtc_detected ? "detected" : "not found",
             st.rtc_ctrl1);
}

static void ui_refresh_pages(void *ctx)
{
    (void)ctx;
    char can_text[420];
    char sensor_text[160];
    ui_build_can_text(can_text, sizeof(can_text));
    ui_build_sensor_text(sensor_text, sizeof(sensor_text));
    ui_screen_can_set_text(&s_can_screen, can_text);
    ui_screen_sensors_set_text(&s_sensors_screen, sensor_text);
}

static void ui_refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    display_lvgl_lock();
    ui_refresh_pages(NULL);
    display_lvgl_unlock();
}

esp_err_t ui_init(void)
{
    esp_err_t ret = ESP_OK;
    display_lvgl_lock();

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);

    lv_obj_t *tv = lv_tabview_create(scr);
    lv_obj_set_size(tv, lv_pct(100), lv_pct(100));

    lv_obj_t *tab_gauge = lv_tabview_add_tab(tv, "Gauge");
    lv_obj_t *tab_can = lv_tabview_add_tab(tv, "CAN");
    lv_obj_t *tab_sensors = lv_tabview_add_tab(tv, "Sensors");
    lv_obj_t *tab_ota = lv_tabview_add_tab(tv, "OTA");

    ret = ui_screen_gauge_init(&s_gauge_screen, tab_gauge, CONFIG_HPE_UI_SPEED_MAX, CONFIG_HPE_UI_RPM_MAX);
    if (ret != ESP_OK) {
        goto err;
    }
    ret = ui_screen_can_init(&s_can_screen, tab_can, ui_refresh_pages, NULL);
    if (ret != ESP_OK) {
        goto err;
    }
    ret = ui_screen_sensors_init(&s_sensors_screen, tab_sensors);
    if (ret != ESP_OK) {
        goto err;
    }
    ret = ui_screen_ota_init(&s_ota_screen, tab_ota);
    if (ret != ESP_OK) {
        goto err;
    }

    ui_refresh_pages(NULL);
    lv_timer_create(ui_refresh_timer_cb, 1000, NULL);

    display_lvgl_unlock();
    return ESP_OK;

err:
    display_lvgl_unlock();
    return ret;
}

void ui_set_speed_rpm(float speed_kmh, float rpm)
{
    int speed = (int)speed_kmh;
    int rpm_i = (int)rpm;

    display_lvgl_lock();
    ui_screen_gauge_set_values(&s_gauge_screen, speed, rpm_i);
    display_lvgl_unlock();
}

void ui_set_ota_status(const char *state_text, int progress_percent, bool success)
{
    display_lvgl_lock();
    ui_screen_ota_set_status(&s_ota_screen, state_text, progress_percent, success);
    display_lvgl_unlock();
}
