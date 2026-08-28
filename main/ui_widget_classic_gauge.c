#include "ui_widget_classic_gauge.h"

#include <stdio.h>
#include "hpe_fonts.h"
#include "src/draw/lv_draw_arc.h"
#include "src/draw/lv_draw_label.h"
#include "src/draw/lv_draw_line.h"
#include "src/draw/lv_draw_rect.h"
#include "src/misc/lv_math.h"
#include "src/misc/lv_text.h"

#define CLASSIC_DIAL_SIZE 232
#define CLASSIC_SPEED_MAX 120
#define CLASSIC_MAJOR_STEP 20
#define CLASSIC_MINOR_STEP 5

typedef struct {
    int speed_max;
    int speed;
} classic_gauge_state_t;

static int classic_scale_px(int base, int size)
{
    return (base * size) / CLASSIC_DIAL_SIZE;
}

static int classic_speed_to_angle_unwrapped(int speed)
{
    static const int speed_marks[] = {0, 20, 40, 60, 80, 100, 120};
    /*
     * Hand-tuned unwrapped angles to match the reference dial geometry.
     * Coordinates use LVGL trigonometric convention (0=right, 90=down).
     */
    static const int angle_marks[] = {136, 165, 215, 272, 325, 380, 412};

    speed = LV_CLAMP(0, speed, CLASSIC_SPEED_MAX);

    for (size_t i = 1; i < sizeof(speed_marks) / sizeof(speed_marks[0]); ++i) {
        if (speed <= speed_marks[i]) {
            int s0 = speed_marks[i - 1];
            int s1 = speed_marks[i];
            int a0 = angle_marks[i - 1];
            int a1 = angle_marks[i];
            return a0 + ((speed - s0) * (a1 - a0)) / (s1 - s0);
        }
    }

    return angle_marks[(sizeof(angle_marks) / sizeof(angle_marks[0])) - 1];
}

static int classic_wrap_angle(int unwrapped_angle)
{
    int angle = unwrapped_angle % 360;
    if (angle < 0) {
        angle += 360;
    }
    return angle;
}

static lv_point_t classic_polar_to_xy(int cx, int cy, int radius, int angle)
{
    lv_point_t p;
    int32_t c = lv_trigo_cos((int16_t)angle);
    int32_t s = lv_trigo_sin((int16_t)angle);
    p.x = (lv_coord_t)(cx + ((radius * c) >> LV_TRIGO_SHIFT));
    p.y = (lv_coord_t)(cy + ((radius * s) >> LV_TRIGO_SHIFT));
    return p;
}

static void classic_draw_line(lv_layer_t *layer, lv_point_t p1, lv_point_t p2, int width, lv_color_t color)
{
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.p1.x = p1.x;
    dsc.p1.y = p1.y;
    dsc.p2.x = p2.x;
    dsc.p2.y = p2.y;
    dsc.width = width;
    dsc.color = color;
    dsc.opa = LV_OPA_COVER;
    dsc.round_start = 1;
    dsc.round_end = 1;
    lv_draw_line(layer, &dsc);
}

static void classic_draw_text_center(lv_layer_t *layer, const char *text, int x, int y, const lv_font_t *font, lv_color_t color)
{
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.text = text;
    dsc.font = font;
    dsc.color = color;
    dsc.opa = LV_OPA_COVER;

    lv_point_t text_size;
    lv_text_get_size(&text_size, text, font, dsc.letter_space, dsc.line_space, LV_COORD_MAX, LV_TEXT_FLAG_NONE);

    lv_area_t coords;
    coords.x1 = (lv_coord_t)(x - text_size.x / 2);
    coords.y1 = (lv_coord_t)(y - text_size.y / 2);
    coords.x2 = (lv_coord_t)(coords.x1 + text_size.x - 1);
    coords.y2 = (lv_coord_t)(coords.y1 + text_size.y - 1);
    lv_draw_label(layer, &dsc, &coords);
}

static void classic_draw_circle_fill(lv_layer_t *layer, int cx, int cy, int r, lv_color_t color)
{
    lv_draw_fill_dsc_t dsc;
    lv_draw_fill_dsc_init(&dsc);
    dsc.color = color;
    dsc.opa = LV_OPA_COVER;
    dsc.radius = LV_RADIUS_CIRCLE;

    lv_area_t area;
    area.x1 = (lv_coord_t)(cx - r);
    area.y1 = (lv_coord_t)(cy - r);
    area.x2 = (lv_coord_t)(cx + r);
    area.y2 = (lv_coord_t)(cy + r);
    lv_draw_fill(layer, &dsc, &area);
}

static void classic_draw_rounded_rect(lv_layer_t *layer, const lv_area_t *area, int radius, lv_color_t color)
{
    lv_draw_fill_dsc_t dsc;
    lv_draw_fill_dsc_init(&dsc);
    dsc.color = color;
    dsc.opa = LV_OPA_COVER;
    dsc.radius = radius;
    lv_draw_fill(layer, &dsc, area);
}

static void classic_draw_rect(lv_layer_t *layer, const lv_area_t *area, lv_color_t color)
{
    lv_draw_fill_dsc_t dsc;
    lv_draw_fill_dsc_init(&dsc);
    dsc.color = color;
    dsc.opa = LV_OPA_COVER;
    dsc.radius = 0;
    lv_draw_fill(layer, &dsc, area);
}

static void classic_draw_badge(lv_layer_t *layer, int cx, int cy, int size)
{
    int r = classic_scale_px(12, size);
    lv_point_t pts[6];
    for (int i = 0; i < 6; ++i) {
        int a = -90 + i * 60;
        pts[i] = classic_polar_to_xy(cx, cy, r, a);
    }

    for (int i = 0; i < 6; ++i) {
        classic_draw_line(layer, pts[i], pts[(i + 1) % 6], classic_scale_px(2, size), lv_color_hex(0xd9dde2));
    }

    classic_draw_text_center(layer, "S", cx, cy + classic_scale_px(1, size), &lv_font_ddin_regular_14, lv_color_hex(0xd9dde2));
}

static void classic_draw_dial(lv_layer_t *layer, int cx, int cy, int size, int speed_max, int speed)
{
    (void)speed_max;

    const int pivot_y = cy;

    const lv_color_t col_bg = lv_color_hex(0x06080a);
    const lv_color_t col_ring_outer = lv_color_hex(0x191c20);
    const lv_color_t col_ring_inner = lv_color_hex(0x101215);
    const lv_color_t col_tick = lv_color_hex(0xf0f3f7);
    const lv_color_t col_num = lv_color_hex(0xd6b517);
    const lv_color_t col_box = lv_color_hex(0x0a0c0e);
    const lv_color_t col_box_slot = lv_color_hex(0x13161a);
    const lv_color_t col_odo = lv_color_hex(0xf4f7fa);
    const lv_color_t col_unit = lv_color_hex(0xd2d8df);

    classic_draw_circle_fill(layer, cx, cy, size / 2, col_bg);
    classic_draw_circle_fill(layer, cx, cy, classic_scale_px(114, size), col_ring_outer);
    classic_draw_circle_fill(layer, cx, cy, classic_scale_px(111, size), col_ring_inner);
    classic_draw_circle_fill(layer, cx, cy, classic_scale_px(108, size), col_bg);

    speed = LV_CLAMP(0, speed, CLASSIC_SPEED_MAX);

    for (int v = 0; v <= CLASSIC_SPEED_MAX; v += CLASSIC_MINOR_STEP) {
        int a = classic_wrap_angle(classic_speed_to_angle_unwrapped(v));
        bool major = (v % CLASSIC_MAJOR_STEP) == 0;
        int r1 = major ? classic_scale_px(83, size) : classic_scale_px(90, size);
        int r2 = classic_scale_px(104, size);
        lv_point_t p1 = classic_polar_to_xy(cx, pivot_y, r1, a);
        lv_point_t p2 = classic_polar_to_xy(cx, pivot_y, r2, a);
        classic_draw_line(layer, p1, p2, major ? classic_scale_px(3, size) : classic_scale_px(2, size), col_tick);
    }

    for (int v = 0; v <= CLASSIC_SPEED_MAX; v += CLASSIC_MAJOR_STEP) {
        int a = classic_wrap_angle(classic_speed_to_angle_unwrapped(v));
        lv_point_t p = classic_polar_to_xy(cx, pivot_y, classic_scale_px(72, size), a);
        char txt[16];
        snprintf(txt, sizeof(txt), "%d", v);
        classic_draw_text_center(layer, txt, p.x, p.y, &lv_font_ddin_medium_20, col_num);
    }

    lv_area_t odo;
    odo.x1 = (lv_coord_t)(cx - classic_scale_px(34, size));
    odo.y1 = (lv_coord_t)(cy - classic_scale_px(36, size));
    odo.x2 = (lv_coord_t)(cx + classic_scale_px(34, size));
    odo.y2 = (lv_coord_t)(cy - classic_scale_px(13, size));
    classic_draw_rounded_rect(layer, &odo, classic_scale_px(2, size), col_box);

    const int slot_w = classic_scale_px(12, size);
    const int slot_h = classic_scale_px(20, size);
    const int slot_gap = classic_scale_px(1, size);
    const int odo_y = cy - classic_scale_px(34, size);
    const int odo_x_start = cx - ((5 * slot_w + 4 * slot_gap) / 2) + (slot_w / 2);
    for (int i = 0; i < 5; ++i) {
        int slot_cx = odo_x_start + i * (slot_w + slot_gap);
        lv_area_t slot;
        slot.x1 = (lv_coord_t)(slot_cx - slot_w / 2);
        slot.y1 = (lv_coord_t)(odo_y);
        slot.x2 = (lv_coord_t)(slot.x1 + slot_w - 1);
        slot.y2 = (lv_coord_t)(slot.y1 + slot_h - 1);
        classic_draw_rounded_rect(layer, &slot, classic_scale_px(1, size), col_box_slot);
    }

    /* Static odometer look to match analog dial style. */
    classic_draw_text_center(layer, "00001", cx, cy - classic_scale_px(22, size), &lv_font_ddin_regular_16, col_odo);

    int needle_angle = classic_wrap_angle(classic_speed_to_angle_unwrapped(speed));
    lv_point_t p_tip = classic_polar_to_xy(cx, pivot_y, classic_scale_px(84, size), needle_angle);
    lv_point_t p_mid = classic_polar_to_xy(cx, pivot_y, classic_scale_px(16, size), needle_angle);
    lv_point_t p_tail = classic_polar_to_xy(cx, pivot_y, classic_scale_px(44, size), needle_angle + 180);

    classic_draw_line(layer, p_mid, p_tip, classic_scale_px(5, size), lv_color_hex(0xff6130));
    classic_draw_line(layer, p_tail, p_mid, classic_scale_px(5, size), lv_color_hex(0xf6f8fb));

    classic_draw_circle_fill(layer, cx, pivot_y, classic_scale_px(11, size), lv_color_hex(0x0f1215));
    classic_draw_circle_fill(layer, cx, pivot_y, classic_scale_px(7, size), lv_color_hex(0x1d2227));

    classic_draw_text_center(layer, "km/h", cx, cy + classic_scale_px(34, size), &lv_font_ddin_regular_16, col_unit);
    classic_draw_badge(layer, cx, cy + classic_scale_px(63, size), size);
}

static void classic_gauge_draw_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target_obj(e);
    classic_gauge_state_t *state = (classic_gauge_state_t *)lv_obj_get_user_data(obj);

    if (code == LV_EVENT_DELETE) {
        if (state) {
            lv_free(state);
            lv_obj_set_user_data(obj, NULL);
        }
        return;
    }

    if (code != LV_EVENT_DRAW_MAIN || !state) {
        return;
    }

    lv_layer_t *layer = lv_event_get_layer(e);
    if (!layer) {
        return;
    }

    lv_area_t area;
    lv_obj_get_content_coords(obj, &area);
    int w = lv_area_get_width(&area);
    int h = lv_area_get_height(&area);
    int size = (w < h) ? w : h;
    int cx = area.x1 + (w / 2);
    int cy = area.y1 + (h / 2);

    classic_draw_rect(layer, &area, lv_color_hex(0x050608));
    classic_draw_dial(layer, cx, cy, size, state->speed_max, state->speed);
}

lv_obj_t *ui_widget_classic_gauge_create(lv_obj_t *parent, int speed_max)
{
    (void)speed_max;

    classic_gauge_state_t *state = lv_malloc(sizeof(classic_gauge_state_t));
    if (!state) {
        return NULL;
    }

    state->speed_max = CLASSIC_SPEED_MAX;
    state->speed = 0;

    lv_obj_t *obj = lv_obj_create(parent);
    if (!obj) {
        lv_free(state);
        return NULL;
    }

    lv_obj_set_size(obj, 232, 232);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x050608), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_outline_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_user_data(obj, state);
    lv_obj_add_event_cb(obj, classic_gauge_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(obj, classic_gauge_draw_cb, LV_EVENT_DELETE, NULL);
    lv_obj_invalidate(obj);

    return obj;
}

void ui_widget_classic_gauge_set_value(lv_obj_t *widget, int speed)
{
    if (!widget) {
        return;
    }

    classic_gauge_state_t *state = (classic_gauge_state_t *)lv_obj_get_user_data(widget);
    if (!state) {
        return;
    }

    state->speed = LV_CLAMP(0, speed, state->speed_max);
    lv_obj_invalidate(widget);
}
