#include "ui.h"

#include <stdio.h>
#include <string.h>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "can_cfg.h"
#include "display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "hpe_fonts.h"
#include "lvgl.h"
#include "widgets/canvas/lv_canvas.h"
#include "raycaster_game.h"
#include "sdkconfig.h"
#include "sensor_rtc.h"
#include "ui_screen_can.h"
#include "ui_screen_color_test.h"
#include "ui_screen_gauge.h"
#include "ui_screen_gauge_classic.h"
#include "ui_screen_ota.h"
#include "ui_screen_sensors.h"

static ui_screen_gauge_t s_gauge_screen;
static ui_screen_gauge_classic_t s_classic_gauge_screen;
static ui_screen_color_test_t s_color_test_screen;
static ui_screen_can_t s_can_screen;
static ui_screen_sensors_t s_sensors_screen;
static ui_screen_ota_t s_ota_screen;

static lv_obj_t *s_raycaster_canvas;
static uint16_t *s_raycaster_frame = NULL;
static uint16_t *s_raycaster_render_frame = NULL;
static bool s_raycaster_started;
static TaskHandle_t s_raycaster_task_handle = NULL;

typedef enum {
    UI_SCREEN_GAUGE = 0,
    UI_SCREEN_GAUGE_CLASSIC,
    UI_SCREEN_COLOR_TEST,
    UI_SCREEN_CAN,
    UI_SCREEN_SENSORS,
    UI_SCREEN_OTA,
    UI_SCREEN_RAYCASTER,
    UI_SCREEN_COUNT,
} ui_screen_id_t;

static lv_obj_t *s_round_root;
static lv_obj_t *s_screen_roots[UI_SCREEN_COUNT];
static lv_obj_t *s_menu_backdrop;
static lv_obj_t *s_menu_panel;
static ui_screen_id_t s_active_screen = UI_SCREEN_GAUGE;
static ui_screen_id_t s_pending_screen_switch = UI_SCREEN_GAUGE;
static bool s_pending_screen_switch_valid = false;
static const char *TAG = "ui";
static portMUX_TYPE s_ui_state_lock = portMUX_INITIALIZER_UNLOCKED;
static int s_pending_speed;
static int s_pending_rpm;
static bool s_pending_gauge_dirty;
static char s_pending_ota_state[24] = "idle";
static int s_pending_ota_progress;
static bool s_pending_ota_success;
static bool s_pending_ota_dirty;
static bool s_demo_sweep_enabled = true;
static int s_demo_speed;
static int s_demo_dir = 1;
static uint32_t s_last_raycaster_update_ms = 0;

static void ui_refresh_pages(void *ctx);
static void ui_show_menu(bool show);
static void ui_switch_screen(ui_screen_id_t screen_id);
static void ui_raycaster_worker_task(void *arg);
static void ui_request_screen_switch(ui_screen_id_t screen_id)
{
    if (screen_id >= UI_SCREEN_COUNT) {
        return;
    }

    portENTER_CRITICAL(&s_ui_state_lock);
    s_pending_screen_switch = screen_id;
    s_pending_screen_switch_valid = true;
    portEXIT_CRITICAL(&s_ui_state_lock);
}

static void ui_raycaster_start_if_needed(void)
{
    if (!s_raycaster_started) {
        raycaster_game_init();
        s_raycaster_started = true;
    }
}

static void ui_raycaster_update(void)
{
    if (!s_raycaster_canvas || !s_raycaster_frame || !s_raycaster_render_frame) {
        return;
    }

    raycaster_input_t input = {0};
    display_touch_state_t touch = {0};
    if (display_get_touch_state(&touch) == ESP_OK && touch.pressed) {
        input.touched = true;
        input.turn_delta_x = touch.dx;

        const int center_x = 120;
        const int center_y = 80;
        const int x_dead = 30;
        const int y_dead = 26;

        if (touch.x < center_x - x_dead) {
            input.turn_left = true;
        } else if (touch.x > center_x + x_dead) {
            input.turn_right = true;
        }

        if (touch.y < center_y - y_dead) {
            input.forward = true;
        } else if (touch.y > center_y + y_dead) {
            input.backward = true;
        }

        if (abs(touch.x - center_x) <= x_dead && abs(touch.y - center_y) <= y_dead) {
            input.fire = true;
        }
    }

    ui_raycaster_start_if_needed();
    raycaster_game_step(&input, s_raycaster_render_frame, 240 * 160 * sizeof(uint16_t));

    /* lv_async_call()/LVGL object APIs are not thread-safe against the LVGL
     * task's lv_timer_handler() running on the other core, so the canvas
     * buffer swap and invalidate must happen under the LVGL mutex here. */
    display_lvgl_lock();
    memcpy(s_raycaster_frame, s_raycaster_render_frame, 240 * 160 * sizeof(uint16_t));
    lv_obj_move_foreground(s_raycaster_canvas);
    lv_obj_invalidate(s_raycaster_canvas);
    display_lvgl_unlock();
}

static void ui_raycaster_worker_task(void *arg)
{
    (void)arg;
    while (true) {
        if (s_active_screen == UI_SCREEN_RAYCASTER) {
            ui_raycaster_update();
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void ui_debug_dump_raycaster_frame(bool use_render_buffer)
{
    const uint16_t *frame = use_render_buffer ? s_raycaster_render_frame : s_raycaster_frame;
    if (frame == NULL) {
        printf("raybuf: no framebuffer allocated yet (%s buffer)\n",
               use_render_buffer ? "render" : "canvas");
        return;
    }

    printf("raybuf: %s framebuffer preview (240x160 RGB565, sampled 16x10)\n",
           use_render_buffer ? "render" : "canvas");

    for (int y = 0; y < 10; ++y) {
        char row[64] = {0};
        char *p = row;

        for (int x = 0; x < 16; ++x) {
            int sx = x * (240 / 16);
            int sy = y * (160 / 10);
            int idx = sy * 240 + sx;
            uint16_t color = frame[idx];

            uint16_t r = (color >> 11) & 0x1F;
            uint16_t g = (color >> 5) & 0x3F;
            uint16_t b = color & 0x1F;
            int brightness = (r * 255 / 31) + (g * 255 / 63) + (b * 255 / 31);
            brightness /= 3;

            const char *glyph = ".";
            if (brightness > 220) glyph = "@";
            else if (brightness > 170) glyph = "#";
            else if (brightness > 120) glyph = "+";
            else if (brightness > 70) glyph = ":";
            else if (brightness > 30) glyph = ".";

            p += snprintf(p, sizeof(row) - (size_t)(p - row), "%s", glyph);
        }

        printf("%s\n", row);
    }

    printf("raybuf: border check -> left=%04x mid=%04x right=%04x center=%04x right-half=%04x\n",
           frame[80 * 240 + 0],
           frame[80 * 240 + 120],
           frame[80 * 240 + 239],
           frame[80 * 240 + 120],
           frame[80 * 240 + 180]);
}

static void ui_apply_demo_sweep(int *speed, int *rpm, bool *gauge_dirty)
{
    if (!s_demo_sweep_enabled || !speed || !rpm || !gauge_dirty) {
        return;
    }

    int speed_max = (CONFIG_HPE_UI_SPEED_MAX > 0) ? CONFIG_HPE_UI_SPEED_MAX : 120;
    int rpm_max = (CONFIG_HPE_UI_RPM_MAX > 0) ? CONFIG_HPE_UI_RPM_MAX : 9000;

    s_demo_speed += s_demo_dir;
    if (s_demo_speed >= speed_max) {
        s_demo_speed = speed_max;
        s_demo_dir = -1;
    } else if (s_demo_speed <= 0) {
        s_demo_speed = 0;
        s_demo_dir = 1;
    }

    *speed = s_demo_speed;
    *rpm = (speed_max > 0) ? ((s_demo_speed * rpm_max) / speed_max) : 0;
    *gauge_dirty = true;
}

static void ui_color_test_done_cb(void *ctx)
{
    (void)ctx;
    ui_show_menu(true);
}

static void ui_set_event_bubble_recursive(lv_obj_t *obj)
{
    if (!obj) {
        return;
    }

    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
    uint32_t child_count = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < child_count; ++i) {
        ui_set_event_bubble_recursive(lv_obj_get_child(obj, i));
    }
}

esp_err_t ui_switch_screen_name(const char *screen_name)
{
    static const struct {
        const char *name;
        ui_screen_id_t screen_id;
    } screen_map[] = {
        {"gauge", UI_SCREEN_GAUGE},
        {"classic", UI_SCREEN_GAUGE_CLASSIC},
        {"color", UI_SCREEN_COLOR_TEST},
        {"can", UI_SCREEN_CAN},
        {"sensors", UI_SCREEN_SENSORS},
        {"ota", UI_SCREEN_OTA},
        {"raycaster", UI_SCREEN_RAYCASTER},
    };

    if (!screen_name) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < sizeof(screen_map) / sizeof(screen_map[0]); ++i) {
        if (strcmp(screen_name, screen_map[i].name) == 0) {
            ui_request_screen_switch(screen_map[i].screen_id);
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

static void ui_show_menu(bool show)
{
    if (!s_menu_backdrop) {
        return;
    }

    if (show) {
        lv_obj_remove_flag(s_menu_backdrop, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_menu_backdrop);
    } else {
        lv_obj_add_flag(s_menu_backdrop, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ui_switch_screen(ui_screen_id_t screen_id)
{
    if (screen_id >= UI_SCREEN_COUNT) {
        return;
    }

    for (int i = 0; i < UI_SCREEN_COUNT; ++i) {
        if (!s_screen_roots[i]) {
            continue;
        }

        if (i == (int)screen_id) {
            lv_obj_remove_flag(s_screen_roots[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_screen_roots[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (screen_id == UI_SCREEN_RAYCASTER) {
        ui_raycaster_start_if_needed();
        s_last_raycaster_update_ms = 0U;
        if (s_raycaster_canvas) {
            lv_obj_move_foreground(s_raycaster_canvas);
            lv_obj_invalidate(s_raycaster_canvas);
        }
    } else {
        s_last_raycaster_update_ms = 0U;
    }

    s_active_screen = screen_id;
    ui_show_menu(false);
}

static void ui_menu_open_cb(lv_event_t *e)
{
    (void)e;
    if (s_active_screen == UI_SCREEN_RAYCASTER) {
        return;
    }

    ESP_LOGI(TAG, "screen tap detected, opening menu");
    ui_show_menu(true);
}

static void ui_menu_backdrop_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    if (target == s_menu_backdrop) {
        ui_show_menu(false);
    }
}

static void ui_menu_item_cb(lv_event_t *e)
{
    uintptr_t id = (uintptr_t)lv_event_get_user_data(e);
    ui_switch_screen((ui_screen_id_t)id);
}

static lv_obj_t *ui_create_screen_menu_item(lv_obj_t *parent, const char *title, ui_screen_id_t id)
{
    lv_obj_t *item = lv_obj_create(parent);
    if (!item) {
        return NULL;
    }

    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(item, lv_pct(100), 30);
    lv_obj_set_style_radius(item, 10, 0);
    lv_obj_set_style_bg_color(item, lv_color_hex(0x13222c), 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(item, 1, 0);
    lv_obj_set_style_border_color(item, lv_color_hex(0x1eaee6), 0);
    lv_obj_set_style_pad_hor(item, 10, 0);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(item, ui_menu_item_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)id);

    lv_obj_t *label = lv_label_create(item);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, lv_color_hex(0xe7f6ff), 0);
    lv_obj_set_style_text_font(label, &lv_font_ddin_regular_14, 0);
    lv_obj_center(label);

    return item;
}

static esp_err_t ui_build_screen_menu(lv_obj_t *parent)
{
    s_menu_backdrop = lv_obj_create(parent);
    ESP_RETURN_ON_FALSE(s_menu_backdrop != NULL, ESP_ERR_NO_MEM, TAG, "menu backdrop create failed");
    lv_obj_set_size(s_menu_backdrop, 240, 240);
    lv_obj_center(s_menu_backdrop);
    lv_obj_set_style_radius(s_menu_backdrop, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_menu_backdrop, lv_color_hex(0x020304), 0);
    lv_obj_set_style_bg_opa(s_menu_backdrop, LV_OPA_60, 0);
    lv_obj_set_style_border_width(s_menu_backdrop, 0, 0);
    lv_obj_set_style_pad_all(s_menu_backdrop, 0, 0);
    lv_obj_add_flag(s_menu_backdrop, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_menu_backdrop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_menu_backdrop, ui_menu_backdrop_cb, LV_EVENT_CLICKED, NULL);

    s_menu_panel = lv_obj_create(s_menu_backdrop);
    ESP_RETURN_ON_FALSE(s_menu_panel != NULL, ESP_ERR_NO_MEM, TAG, "menu panel create failed");
    lv_obj_set_size(s_menu_panel, 184, 206);
    lv_obj_center(s_menu_panel);
    lv_obj_set_style_radius(s_menu_panel, 18, 0);
    lv_obj_set_style_bg_color(s_menu_panel, lv_color_hex(0x0b1319), 0);
    lv_obj_set_style_bg_opa(s_menu_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_menu_panel, 2, 0);
    lv_obj_set_style_border_color(s_menu_panel, lv_color_hex(0x1db9f3), 0);
    lv_obj_set_style_pad_all(s_menu_panel, 10, 0);
    lv_obj_set_flex_flow(s_menu_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_menu_panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(s_menu_panel, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *title = lv_label_create(s_menu_panel);
    ESP_RETURN_ON_FALSE(title != NULL, ESP_ERR_NO_MEM, TAG, "menu title create failed");
    lv_label_set_text(title, "Open Screen");
    lv_obj_set_style_text_color(title, lv_color_hex(0x7fdcff), 0);
    lv_obj_set_style_text_font(title, &lv_font_ddin_regular_16, 0);
    lv_obj_set_style_pad_bottom(title, 4, 0);

    ESP_RETURN_ON_FALSE(ui_create_screen_menu_item(s_menu_panel, "Gauge", UI_SCREEN_GAUGE) != NULL,
                        ESP_ERR_NO_MEM, TAG, "menu item create failed");
    ESP_RETURN_ON_FALSE(ui_create_screen_menu_item(s_menu_panel, "Gauge Classic", UI_SCREEN_GAUGE_CLASSIC) != NULL,
                        ESP_ERR_NO_MEM, TAG, "menu item create failed");
    ESP_RETURN_ON_FALSE(ui_create_screen_menu_item(s_menu_panel, "Color Test", UI_SCREEN_COLOR_TEST) != NULL,
                        ESP_ERR_NO_MEM, TAG, "menu item create failed");
    ESP_RETURN_ON_FALSE(ui_create_screen_menu_item(s_menu_panel, "CAN", UI_SCREEN_CAN) != NULL,
                        ESP_ERR_NO_MEM, TAG, "menu item create failed");
    ESP_RETURN_ON_FALSE(ui_create_screen_menu_item(s_menu_panel, "Sensors", UI_SCREEN_SENSORS) != NULL,
                        ESP_ERR_NO_MEM, TAG, "menu item create failed");
    ESP_RETURN_ON_FALSE(ui_create_screen_menu_item(s_menu_panel, "OTA", UI_SCREEN_OTA) != NULL,
                        ESP_ERR_NO_MEM, TAG, "menu item create failed");
    ESP_RETURN_ON_FALSE(ui_create_screen_menu_item(s_menu_panel, "Raycaster", UI_SCREEN_RAYCASTER) != NULL,
                        ESP_ERR_NO_MEM, TAG, "menu item create failed");

    return ESP_OK;
}

static void ui_build_can_summary(char *out, size_t out_size)
{
    if (!out || out_size == 0) {
        return;
    }

    const can_cfg_t *cfg = can_cfg_get();
    const char *speed_src = "-";
    const char *rpm_src = "-";
    for (uint8_t i = 0; i < cfg->signal_count; ++i) {
        if (strcmp(cfg->signals[i].name, "speed") == 0) {
            speed_src = "ok";
        } else if (strcmp(cfg->signals[i].name, "rpm") == 0) {
            rpm_src = "ok";
        }
    }
    snprintf(out, out_size, "CAN sigs:%u speed:%s rpm:%s", cfg->signal_count, speed_src, rpm_src);
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
    char can_text[96];
    char sensor_text[96];
    ui_build_can_summary(can_text, sizeof(can_text));
    ui_build_sensor_text(sensor_text, sizeof(sensor_text));
    ui_screen_can_set_text(&s_can_screen, can_text);
    ui_screen_sensors_set_text(&s_sensors_screen, sensor_text);
}

static void ui_refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    static uint8_t summary_div = 0;

    int speed = 0;
    int rpm = 0;
    bool gauge_dirty = false;
    char ota_state[24];
    int ota_progress = 0;
    bool ota_success = false;
    bool ota_dirty = false;

    portENTER_CRITICAL(&s_ui_state_lock);
    speed = s_pending_speed;
    rpm = s_pending_rpm;
    gauge_dirty = s_pending_gauge_dirty;
    s_pending_gauge_dirty = false;

    strncpy(ota_state, s_pending_ota_state, sizeof(ota_state) - 1);
    ota_state[sizeof(ota_state) - 1] = '\0';
    ota_progress = s_pending_ota_progress;
    ota_success = s_pending_ota_success;
    ota_dirty = s_pending_ota_dirty;
    s_pending_ota_dirty = false;
    portEXIT_CRITICAL(&s_ui_state_lock);

    ui_apply_demo_sweep(&speed, &rpm, &gauge_dirty);

    if (gauge_dirty) {
        ui_screen_gauge_set_values(&s_gauge_screen, speed, rpm);
        ui_screen_gauge_classic_set_value(&s_classic_gauge_screen, speed);
    }

    if (ota_dirty) {
        ui_screen_ota_set_status(&s_ota_screen, ota_state, ota_progress, ota_success);
    }

    if (s_pending_screen_switch_valid) {
        ui_screen_id_t requested_screen = UI_SCREEN_GAUGE;
        portENTER_CRITICAL(&s_ui_state_lock);
        requested_screen = s_pending_screen_switch;
        s_pending_screen_switch_valid = false;
        portEXIT_CRITICAL(&s_ui_state_lock);
        ui_switch_screen(requested_screen);
    }

    if (++summary_div >= 10) {
        summary_div = 0;
        ui_refresh_pages(NULL);
    }
}

esp_err_t ui_init(void)
{
    esp_err_t ret = ESP_OK;
    ESP_LOGI(TAG, "building round UI");

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x050608), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *round_root = lv_obj_create(scr);
    lv_obj_set_size(round_root, 240, 240);
    lv_obj_center(round_root);
    lv_obj_set_style_radius(round_root, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(round_root, false, 0);
    lv_obj_set_style_border_width(round_root, 0, 0);
    lv_obj_set_style_bg_color(round_root, lv_color_hex(0x050608), 0);
    lv_obj_set_style_bg_opa(round_root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(round_root, 0, 0);
    lv_obj_set_scrollbar_mode(round_root, LV_SCROLLBAR_MODE_OFF);
    s_round_root = round_root;

    for (int i = 0; i < UI_SCREEN_COUNT; ++i) {
        s_screen_roots[i] = lv_obj_create(round_root);
        ESP_RETURN_ON_FALSE(s_screen_roots[i] != NULL, ESP_ERR_NO_MEM, TAG, "screen root create failed");
        lv_obj_set_size(s_screen_roots[i], 240, 240);
        lv_obj_center(s_screen_roots[i]);
        lv_obj_set_style_bg_color(s_screen_roots[i], lv_color_hex(0x050608), 0);
        lv_obj_set_style_bg_opa(s_screen_roots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_screen_roots[i], 0, 0);
        lv_obj_set_style_outline_width(s_screen_roots[i], 0, 0);
        lv_obj_set_style_shadow_width(s_screen_roots[i], 0, 0);
        lv_obj_set_style_pad_all(s_screen_roots[i], 0, 0);
        lv_obj_set_scrollbar_mode(s_screen_roots[i], LV_SCROLLBAR_MODE_OFF);
        if (i != UI_SCREEN_COLOR_TEST && i != UI_SCREEN_RAYCASTER) {
            lv_obj_add_flag(s_screen_roots[i], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(s_screen_roots[i], ui_menu_open_cb, LV_EVENT_CLICKED, NULL);
        }
    }

    ret = ui_screen_gauge_init(&s_gauge_screen, s_screen_roots[UI_SCREEN_GAUGE], CONFIG_HPE_UI_SPEED_MAX, CONFIG_HPE_UI_RPM_MAX);
    if (ret != ESP_OK) {
        goto err;
    }

    ret = ui_screen_gauge_classic_init(&s_classic_gauge_screen, s_screen_roots[UI_SCREEN_GAUGE_CLASSIC], CONFIG_HPE_UI_SPEED_MAX);
    if (ret != ESP_OK) {
        goto err;
    }

    ret = ui_screen_color_test_init(&s_color_test_screen,
                                    s_screen_roots[UI_SCREEN_COLOR_TEST],
                                    ui_color_test_done_cb,
                                    NULL);
    if (ret != ESP_OK) {
        goto err;
    }

    ret = ui_screen_can_init(&s_can_screen, s_screen_roots[UI_SCREEN_CAN], ui_refresh_pages, NULL);
    if (ret != ESP_OK) {
        goto err;
    }

    ret = ui_screen_sensors_init(&s_sensors_screen, s_screen_roots[UI_SCREEN_SENSORS]);
    if (ret != ESP_OK) {
        goto err;
    }

    ret = ui_screen_ota_init(&s_ota_screen, s_screen_roots[UI_SCREEN_OTA]);
    if (ret != ESP_OK) {
        goto err;
    }

    s_raycaster_frame = heap_caps_malloc(240 * 160 * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_raycaster_render_frame = heap_caps_malloc(240 * 160 * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_raycaster_frame || !s_raycaster_render_frame) {
        ret = ESP_ERR_NO_MEM;
        goto err;
    }

    s_raycaster_canvas = lv_canvas_create(s_screen_roots[UI_SCREEN_RAYCASTER]);
    if (s_raycaster_canvas == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto err;
    }
    lv_obj_set_size(s_raycaster_canvas, 240, 160);
    lv_obj_align(s_raycaster_canvas, LV_ALIGN_CENTER, 0, 0);
    lv_canvas_set_buffer(s_raycaster_canvas, s_raycaster_frame, 240, 160, LV_COLOR_FORMAT_RGB565);
    lv_obj_t *ray_title = lv_label_create(s_screen_roots[UI_SCREEN_RAYCASTER]);
    if (ray_title == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto err;
    }
    lv_label_set_text(ray_title, "Raycaster");
    lv_obj_set_style_text_font(ray_title, &lv_font_ddin_regular_16, 0);
    lv_obj_set_style_text_color(ray_title, lv_color_hex(0x7fdcff), 0);
    lv_obj_align(ray_title, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_t *ray_hint = lv_label_create(s_screen_roots[UI_SCREEN_RAYCASTER]);
    if (ray_hint == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto err;
    }
    lv_label_set_text(ray_hint, "Touch: drag to turn, right side to fire");
    lv_obj_set_style_text_font(ray_hint, &lv_font_ddin_regular_14, 0);
    lv_obj_set_style_text_color(ray_hint, lv_color_hex(0xd8ebff), 0);
    lv_obj_align(ray_hint, LV_ALIGN_BOTTOM_MID, 0, -4);

    ui_set_event_bubble_recursive(s_screen_roots[UI_SCREEN_GAUGE]);
    ui_set_event_bubble_recursive(s_screen_roots[UI_SCREEN_GAUGE_CLASSIC]);
    ui_set_event_bubble_recursive(s_screen_roots[UI_SCREEN_COLOR_TEST]);
    ui_set_event_bubble_recursive(s_screen_roots[UI_SCREEN_CAN]);
    ui_set_event_bubble_recursive(s_screen_roots[UI_SCREEN_SENSORS]);
    ui_set_event_bubble_recursive(s_screen_roots[UI_SCREEN_OTA]);
    ui_set_event_bubble_recursive(s_screen_roots[UI_SCREEN_RAYCASTER]);

    ret = ui_build_screen_menu(s_round_root);
    if (ret != ESP_OK) {
        goto err;
    }

    ui_switch_screen(UI_SCREEN_GAUGE);

    // Render a non-zero gauge state at boot so the UI is visibly alive.
    ui_screen_gauge_set_values(&s_gauge_screen, 24, 1200);
    ui_screen_gauge_classic_set_value(&s_classic_gauge_screen, 24);

    portENTER_CRITICAL(&s_ui_state_lock);
    s_demo_speed = 24;
    s_demo_dir = 1;
    s_pending_speed = s_demo_speed;
    s_pending_rpm = 1200;
    s_pending_gauge_dirty = false;
    strncpy(s_pending_ota_state, "idle", sizeof(s_pending_ota_state) - 1);
    s_pending_ota_state[sizeof(s_pending_ota_state) - 1] = '\0';
    s_pending_ota_progress = 0;
    s_pending_ota_success = false;
    s_pending_ota_dirty = false;
    portEXIT_CRITICAL(&s_ui_state_lock);

    ui_refresh_pages(NULL);
    ui_screen_ota_set_status(&s_ota_screen, "idle", 0, false);
    lv_timer_create(ui_refresh_timer_cb, 33, NULL);
    if (xTaskCreatePinnedToCore(ui_raycaster_worker_task, "raycaster_worker", 8192, NULL, 3, &s_raycaster_task_handle, 0) != pdPASS) {
        ESP_LOGW(TAG, "raycaster worker task create failed, continuing with timer-based refresh");
    }
    ESP_LOGI(TAG, "round UI ready");
    return ESP_OK;

err:
    ESP_LOGE(TAG, "ui init failed: %s", esp_err_to_name(ret));
    return ret;
}

void ui_set_speed_rpm(float speed_kmh, float rpm)
{
    int speed = (int)speed_kmh;
    int rpm_i = (int)rpm;

    portENTER_CRITICAL(&s_ui_state_lock);
    s_pending_speed = speed;
    s_pending_rpm = rpm_i;
    s_pending_gauge_dirty = true;
    portEXIT_CRITICAL(&s_ui_state_lock);
}

void ui_set_ota_status(const char *state_text, int progress_percent, bool success)
{
    if (progress_percent < 0) {
        progress_percent = 0;
    }
    if (progress_percent > 100) {
        progress_percent = 100;
    }

    portENTER_CRITICAL(&s_ui_state_lock);
    strncpy(s_pending_ota_state, state_text ? state_text : "idle", sizeof(s_pending_ota_state) - 1);
    s_pending_ota_state[sizeof(s_pending_ota_state) - 1] = '\0';
    s_pending_ota_progress = progress_percent;
    s_pending_ota_success = success;
    s_pending_ota_dirty = true;
    portEXIT_CRITICAL(&s_ui_state_lock);
}
