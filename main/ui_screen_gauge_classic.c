#include "ui_screen_gauge_classic.h"

#include "esp_check.h"
#include "ui_widget_classic_gauge.h"

esp_err_t ui_screen_gauge_classic_init(ui_screen_gauge_classic_t *screen, lv_obj_t *parent, int speed_max)
{
    ESP_RETURN_ON_FALSE(screen && parent, ESP_ERR_INVALID_ARG, "ui_screen_gauge_classic", "invalid args");

    screen->root = parent;
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x050608), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    screen->gauge = ui_widget_classic_gauge_create(parent, speed_max);
    ESP_RETURN_ON_FALSE(screen->gauge != NULL, ESP_ERR_NO_MEM, "ui_screen_gauge_classic", "gauge widget create failed");
    lv_obj_center(screen->gauge);
    return ESP_OK;
}

void ui_screen_gauge_classic_set_value(ui_screen_gauge_classic_t *screen, int speed)
{
    if (!screen || !screen->gauge) {
        return;
    }
    ui_widget_classic_gauge_set_value(screen->gauge, speed);
}
