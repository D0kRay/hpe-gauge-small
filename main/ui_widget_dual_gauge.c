#include "ui_widget_dual_gauge.h"

#include <stdio.h>
#include "hpe_fonts.h"
#include "src/draw/lv_draw_arc.h"
#include "src/draw/lv_draw_line.h"
#include "src/draw/lv_draw_label.h"
#include "src/draw/lv_draw_rect.h"
#include "src/misc/lv_math.h"
#include "src/misc/lv_text.h"

#define DIAL_SIZE 232
#define GAUGE_START_ANGLE 135
#define GAUGE_SWEEP_ANGLE 270

typedef struct {
    int speed_max;
    int rpm_max;
    int speed;
    int rpm;
} gauge_draw_state_t;

static int normalize_angle(int angle)
{
    while (angle < 0) {
        angle += 360;
    }
    while (angle >= 360) {
        angle -= 360;
    }
    return angle;
}

static int speed_to_angle(int speed, int speed_max)
{
    if (speed_max <= 0) {
        return GAUGE_START_ANGLE;
    }
    return GAUGE_START_ANGLE + (speed * GAUGE_SWEEP_ANGLE) / speed_max;
}

static int scale_px(int base, int size)
{
    return (base * size) / DIAL_SIZE;
}

static lv_point_t polar_to_xy(int cx, int cy, int radius, int angle)
{
    lv_point_t p;
    int32_t c = lv_trigo_cos((int16_t)angle);
    int32_t s = lv_trigo_sin((int16_t)angle);
    p.x = (lv_coord_t)(cx + ((radius * c) >> LV_TRIGO_SHIFT));
    p.y = (lv_coord_t)(cy + ((radius * s) >> LV_TRIGO_SHIFT));
    return p;
}

static void draw_arc_segment(lv_layer_t *layer, int cx, int cy, int radius,
                             int start_angle, int end_angle,
                             int width, lv_color_t color, lv_opa_t opa, bool rounded)
{
    int s = start_angle;
    int e = end_angle;
    while (s < 0) {
        s += 360;
        e += 360;
    }
    while (s >= 360) {
        s -= 360;
        e -= 360;
    }

    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.center.x = (lv_coord_t)cx;
    dsc.center.y = (lv_coord_t)cy;
    dsc.radius = (uint16_t)radius;
    dsc.width = width;
    dsc.color = color;
    dsc.opa = opa;
    dsc.rounded = rounded ? 1 : 0;

    if (e <= 360) {
        dsc.start_angle = s;
        dsc.end_angle = e;
        lv_draw_arc(layer, &dsc);
    } else {
        dsc.start_angle = s;
        dsc.end_angle = 359;
        lv_draw_arc(layer, &dsc);
        dsc.start_angle = 0;
        dsc.end_angle = e - 360;
        lv_draw_arc(layer, &dsc);
    }
}

static void draw_line_segment(lv_layer_t *layer, lv_point_t p1, lv_point_t p2,
                              int width, lv_color_t color, lv_opa_t opa)
{
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.p1.x = p1.x;
    dsc.p1.y = p1.y;
    dsc.p2.x = p2.x;
    dsc.p2.y = p2.y;
    dsc.width = width;
    dsc.color = color;
    dsc.opa = opa;
    dsc.round_start = 1;
    dsc.round_end = 1;
    lv_draw_line(layer, &dsc);
}

static void draw_text_center(lv_layer_t *layer, const char *text, int x, int y,
                             const lv_font_t *font, lv_color_t color)
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

static void draw_filled_circle(lv_layer_t *layer, int cx, int cy, int r, lv_color_t color)
{
    lv_draw_fill_dsc_t dsc;
    lv_draw_fill_dsc_init(&dsc);
    dsc.color = color;
    dsc.opa = LV_OPA_COVER;
    dsc.radius = LV_RADIUS_CIRCLE;

    lv_area_t a;
    a.x1 = (lv_coord_t)(cx - r);
    a.y1 = (lv_coord_t)(cy - r);
    a.x2 = (lv_coord_t)(cx + r);
    a.y2 = (lv_coord_t)(cy + r);
    lv_draw_fill(layer, &dsc, &a);
}

static void draw_filled_rect(lv_layer_t *layer, const lv_area_t *a, lv_color_t color)
{
    lv_draw_fill_dsc_t dsc;
    lv_draw_fill_dsc_init(&dsc);
    dsc.color = color;
    dsc.opa = LV_OPA_COVER;
    dsc.radius = 0;
    lv_draw_fill(layer, &dsc, a);
}

static void draw_speedometer_layer(lv_layer_t *layer, int cx, int cy, int size, int speed_max, int rpm_max, int speed, int rpm)
{
    if (!layer) {
        return;
    }

    speed = LV_CLAMP(0, speed, speed_max);
    rpm = LV_CLAMP(0, rpm, rpm_max);

    draw_arc_segment(layer, cx, cy, scale_px(114, size), 0, 359, scale_px(2, size), lv_color_hex(0x818791), LV_OPA_COVER, false);
    draw_arc_segment(layer, cx, cy, scale_px(111, size), GAUGE_START_ANGLE, GAUGE_START_ANGLE + GAUGE_SWEEP_ANGLE,
                     3, lv_color_hex(0x12b8ff), LV_OPA_COVER, false);
    draw_arc_segment(layer, cx, cy, scale_px(111, size), speed_to_angle(205, speed_max), speed_to_angle(speed_max, speed_max),
                     4, lv_color_hex(0xff3a28), LV_OPA_COVER, false);

    for (int v = 0; v <= speed_max; v += 10) {
        int a = speed_to_angle(v, speed_max);
        bool major = (v % 20) == 0;
        int r1 = major ? scale_px(89, size) : scale_px(96, size);
        int r2 = scale_px(107, size);
        lv_point_t p1 = polar_to_xy(cx, cy, r1, a);
        lv_point_t p2 = polar_to_xy(cx, cy, r2, a);
        lv_color_t c = major ? lv_color_hex(0x20c4ff) : lv_color_hex(0x3f444b);
        draw_line_segment(layer, p1, p2, major ? scale_px(2, size) : scale_px(1, size), c, LV_OPA_COVER);
    }

    for (int v = 0; v <= speed_max; v += 20) {
        int a = speed_to_angle(v, speed_max);
        lv_point_t p = polar_to_xy(cx, cy, scale_px(77, size), a);
        char txt[16];
        snprintf(txt, sizeof(txt), "%d", v);
        draw_text_center(layer, txt, p.x, p.y, &lv_font_ddin_regular_16, lv_color_hex(0xeef2f6));
    }

    int rpm_segment_count = 16;
    int rpm_active = (rpm * rpm_segment_count) / rpm_max;
    for (int i = 0; i < rpm_segment_count; ++i) {
        int seg_start = 300 + i * 10;
        int seg_end = seg_start + 6;
        lv_color_t col = (i < rpm_active) ? lv_color_hex(0x14b7ff) : lv_color_hex(0x1f2328);
        draw_arc_segment(layer, cx, cy, scale_px(56, size), seg_start, seg_end, scale_px(8, size), col, LV_OPA_COVER, true);
    }

    int needle_angle = speed_to_angle(speed, speed_max);
    lv_point_t p_tip = polar_to_xy(cx, cy, scale_px(96, size), needle_angle);
    lv_point_t p_tail = polar_to_xy(cx, cy, scale_px(16, size), normalize_angle(needle_angle + 180));
    draw_line_segment(layer, p_tail, p_tip, scale_px(4, size), lv_color_hex(0xff5a00), LV_OPA_COVER);

    draw_filled_circle(layer, cx, cy, scale_px(11, size), lv_color_hex(0xe8edf2));
    draw_filled_circle(layer, cx, cy, scale_px(8, size), lv_color_hex(0x101318));
    draw_filled_circle(layer, cx, cy, scale_px(3, size), lv_color_hex(0x2f343b));

    char speed_txt[16];
    char rpm_txt[16];
    snprintf(speed_txt, sizeof(speed_txt), "%d", speed);
    snprintf(rpm_txt, sizeof(rpm_txt), "%d", rpm / 100);
    draw_text_center(layer, speed_txt, cx - scale_px(58, size), cy + scale_px(66, size), &lv_font_ddin_bold_20, lv_color_hex(0xf5f7fa));
    draw_text_center(layer, "km/h", cx - scale_px(58, size), cy + scale_px(90, size), &lv_font_ddin_regular_14, lv_color_hex(0xc6ccd4));
    draw_text_center(layer, rpm_txt, cx + scale_px(21, size), cy + scale_px(15, size), &lv_font_ddin_regular_14, lv_color_hex(0x737b85));
}

static void gauge_draw_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target_obj(e);
    gauge_draw_state_t *state = (gauge_draw_state_t *)lv_obj_get_user_data(obj);

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

    draw_filled_rect(layer, &area, lv_color_hex(0x050608));
    draw_filled_circle(layer, cx, cy, size / 2, lv_color_hex(0x0a0b0d));
    draw_speedometer_layer(layer, cx, cy, size, state->speed_max, state->rpm_max, state->speed, state->rpm);
}

static gauge_draw_state_t *gauge_state_from_widget(lv_obj_t *widget)
{
    if (!widget) {
        return NULL;
    }
    return (gauge_draw_state_t *)lv_obj_get_user_data(widget);
}

lv_obj_t *ui_widget_dual_gauge_create(lv_obj_t *parent, int speed_max, int rpm_max)
{
    gauge_draw_state_t *state = lv_malloc(sizeof(gauge_draw_state_t));
    if (!state) {
        return NULL;
    }

    state->speed_max = (speed_max > 0) ? speed_max : 240;
    state->rpm_max = (rpm_max > 0) ? rpm_max : 9000;
    state->speed = 0;
    state->rpm = 0;

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
    lv_obj_add_event_cb(obj, gauge_draw_event_cb, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(obj, gauge_draw_event_cb, LV_EVENT_DELETE, NULL);

    lv_obj_invalidate(obj);

    return obj;
}

void ui_widget_dual_gauge_set_values(lv_obj_t *widget, int speed, int rpm)
{
    gauge_draw_state_t *state = gauge_state_from_widget(widget);
    if (!state) {
        return;
    }

    speed = LV_CLAMP(0, speed, state->speed_max);
    rpm = LV_CLAMP(0, rpm, state->rpm_max);
    if (state->speed == speed && state->rpm == rpm) {
        return;
    }

    state->speed = speed;
    state->rpm = rpm;
    lv_obj_invalidate(widget);
}
