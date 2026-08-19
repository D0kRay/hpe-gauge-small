#include "ui_screen_gauge.h"

#include "esp_check.h"
#include "ui_widget_dual_gauge.h"

esp_err_t ui_screen_gauge_init(ui_screen_gauge_t *screen, lv_obj_t *parent, int speed_max, int rpm_max)
{
    ESP_RETURN_ON_FALSE(screen && parent, ESP_ERR_INVALID_ARG, "ui_screen_gauge", "invalid args");

    screen->root = parent;
    screen->dual_gauge = ui_widget_dual_gauge_create(parent, speed_max, rpm_max);
    ESP_RETURN_ON_FALSE(screen->dual_gauge != NULL, ESP_ERR_NO_MEM, "ui_screen_gauge", "gauge widget create failed");
    lv_obj_center(screen->dual_gauge);
    return ESP_OK;
}

void ui_screen_gauge_set_values(ui_screen_gauge_t *screen, int speed, int rpm)
{
    if (!screen || !screen->dual_gauge) {
        return;
    }
    ui_widget_dual_gauge_set_values(screen->dual_gauge, speed, rpm);
}
