#include "ui_screen_ota.h"

#include <stdio.h>
#include "esp_check.h"
#include "ui_widget_info_panel.h"

esp_err_t ui_screen_ota_init(ui_screen_ota_t *screen, lv_obj_t *parent)
{
    ESP_RETURN_ON_FALSE(screen && parent, ESP_ERR_INVALID_ARG, "ui_screen_ota", "invalid args");

    screen->root = parent;
    screen->panel = ui_widget_info_panel_create(parent);
    ESP_RETURN_ON_FALSE(screen->panel != NULL, ESP_ERR_NO_MEM, "ui_screen_ota", "panel create failed");
    lv_obj_set_size(screen->panel, lv_pct(100), lv_pct(100));
    lv_obj_align(screen->panel, LV_ALIGN_TOP_LEFT, 4, 4);
    ui_widget_info_panel_set_title(screen->panel, "OTA update");
    ui_widget_info_panel_set_body(screen->panel, "Waiting for upload from web UI.");
    return ESP_OK;
}

void ui_screen_ota_set_status(ui_screen_ota_t *screen, const char *state_text, int progress_percent, bool success)
{
    if (!screen || !screen->panel) {
        return;
    }

    if (progress_percent < 0) {
        progress_percent = 0;
    }
    if (progress_percent > 100) {
        progress_percent = 100;
    }

    char body[200];
    snprintf(body, sizeof(body),
             "Status: %s\nProgress: %d%%\nResult: %s",
             state_text ? state_text : "idle",
             progress_percent,
             success ? "ready to reboot" : "pending");
    ui_widget_info_panel_set_body(screen->panel, body);
}
