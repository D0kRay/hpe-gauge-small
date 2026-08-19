#include "ui_screen_sensors.h"

#include "esp_check.h"
#include "ui_widget_info_panel.h"

esp_err_t ui_screen_sensors_init(ui_screen_sensors_t *screen, lv_obj_t *parent)
{
    ESP_RETURN_ON_FALSE(screen && parent, ESP_ERR_INVALID_ARG, "ui_screen_sensors", "invalid args");

    screen->root = parent;
    screen->panel = ui_widget_info_panel_create(parent);
    ESP_RETURN_ON_FALSE(screen->panel != NULL, ESP_ERR_NO_MEM, "ui_screen_sensors", "panel create failed");
    lv_obj_set_size(screen->panel, lv_pct(100), lv_pct(100));
    lv_obj_align(screen->panel, LV_ALIGN_TOP_LEFT, 4, 4);
    ui_widget_info_panel_set_title(screen->panel, "Sensors");
    return ESP_OK;
}

void ui_screen_sensors_set_text(ui_screen_sensors_t *screen, const char *text)
{
    if (!screen || !screen->panel) {
        return;
    }
    ui_widget_info_panel_set_body(screen->panel, text);
}
