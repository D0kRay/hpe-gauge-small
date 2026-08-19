#include "ui_screen_can.h"

#include "esp_check.h"
#include "ui_widget_info_panel.h"

static void can_refresh_btn_cb(lv_event_t *e)
{
    ui_screen_can_t *screen = (ui_screen_can_t *)lv_event_get_user_data(e);
    if (screen && screen->refresh_cb) {
        screen->refresh_cb(screen->refresh_ctx);
    }
}

esp_err_t ui_screen_can_init(ui_screen_can_t *screen, lv_obj_t *parent, ui_screen_can_refresh_cb_t refresh_cb, void *refresh_ctx)
{
    ESP_RETURN_ON_FALSE(screen && parent, ESP_ERR_INVALID_ARG, "ui_screen_can", "invalid args");

    screen->root = parent;
    screen->refresh_cb = refresh_cb;
    screen->refresh_ctx = refresh_ctx;

    screen->panel = ui_widget_info_panel_create(parent);
    ESP_RETURN_ON_FALSE(screen->panel != NULL, ESP_ERR_NO_MEM, "ui_screen_can", "panel create failed");
    lv_obj_set_size(screen->panel, lv_pct(100), lv_pct(100));
    lv_obj_align(screen->panel, LV_ALIGN_TOP_LEFT, 4, 4);
    ui_widget_info_panel_set_title(screen->panel, "CAN profile");

    screen->refresh_btn = lv_button_create(parent);
    ESP_RETURN_ON_FALSE(screen->refresh_btn != NULL, ESP_ERR_NO_MEM, "ui_screen_can", "button create failed");
    lv_obj_set_size(screen->refresh_btn, 94, 32);
    lv_obj_align(screen->refresh_btn, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
    lv_obj_add_event_cb(screen->refresh_btn, can_refresh_btn_cb, LV_EVENT_CLICKED, screen);

    lv_obj_t *refresh_lbl = lv_label_create(screen->refresh_btn);
    lv_label_set_text(refresh_lbl, "Refresh");
    lv_obj_center(refresh_lbl);

    return ESP_OK;
}

void ui_screen_can_set_text(ui_screen_can_t *screen, const char *text)
{
    if (!screen || !screen->panel) {
        return;
    }
    ui_widget_info_panel_set_body(screen->panel, text);
}
