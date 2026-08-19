#include "ui_widget_info_panel.h"

typedef struct {
    lv_obj_t obj;
    lv_obj_t *title;
    lv_obj_t *body;
} ui_widget_info_panel_t;

static void info_panel_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    ui_widget_info_panel_t *w = (ui_widget_info_panel_t *)obj;

    lv_obj_set_size(obj, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x1d2b36), 0);
    lv_obj_set_style_radius(obj, 10, 0);
    lv_obj_set_style_pad_all(obj, 8, 0);

    w->title = lv_label_create(obj);
    lv_obj_set_style_text_font(w->title, &lv_font_montserrat_14, 0);
    lv_label_set_text(w->title, "Info");
    lv_obj_align(w->title, LV_ALIGN_TOP_LEFT, 0, 0);

    w->body = lv_label_create(obj);
    lv_obj_set_width(w->body, lv_pct(100));
    lv_label_set_long_mode(w->body, LV_LABEL_LONG_WRAP);
    lv_label_set_text(w->body, "");
    lv_obj_align_to(w->body, w->title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);
}

static const lv_obj_class_t s_info_panel_class = {
    .constructor_cb = info_panel_constructor,
    .instance_size = sizeof(ui_widget_info_panel_t),
    .base_class = &lv_obj_class,
    .name = "ui_info_panel",
};

lv_obj_t *ui_widget_info_panel_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&s_info_panel_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

void ui_widget_info_panel_set_title(lv_obj_t *widget, const char *title)
{
    if (!widget || !title) {
        return;
    }
    ui_widget_info_panel_t *w = (ui_widget_info_panel_t *)widget;
    lv_label_set_text(w->title, title);
}

void ui_widget_info_panel_set_body(lv_obj_t *widget, const char *body)
{
    if (!widget) {
        return;
    }
    ui_widget_info_panel_t *w = (ui_widget_info_panel_t *)widget;
    lv_label_set_text(w->body, body ? body : "");
}
