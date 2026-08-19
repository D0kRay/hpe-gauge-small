#include "ui_widget_info_panel.h"

static lv_obj_t *info_panel_title_obj(lv_obj_t *widget)
{
    return widget ? lv_obj_get_child(widget, 0) : NULL;
}

static lv_obj_t *info_panel_body_obj(lv_obj_t *widget)
{
    return widget ? lv_obj_get_child(widget, 1) : NULL;
}

lv_obj_t *ui_widget_info_panel_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_size(obj, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x1d2b36), 0);
    lv_obj_set_style_radius(obj, 10, 0);
    lv_obj_set_style_pad_all(obj, 8, 0);

    lv_obj_t *title = lv_label_create(obj);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "Info");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *body = lv_label_create(obj);
    lv_obj_set_width(body, lv_pct(100));
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_label_set_text(body, "");
    lv_obj_align_to(body, title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);
    return obj;
}

void ui_widget_info_panel_set_title(lv_obj_t *widget, const char *title)
{
    if (!widget || !title) {
        return;
    }
    lv_obj_t *title_obj = info_panel_title_obj(widget);
    if (title_obj) {
        lv_label_set_text(title_obj, title);
    }
}

void ui_widget_info_panel_set_body(lv_obj_t *widget, const char *body)
{
    if (!widget) {
        return;
    }
    lv_obj_t *body_obj = info_panel_body_obj(widget);
    if (body_obj) {
        lv_label_set_text(body_obj, body ? body : "");
    }
}
