#include "can_cfg.h"
#include "can_console.h"
#include "can_service.h"
#include "display.h"
#include "esp_check.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_log.h"
#include "linenoise/linenoise.h"
#include "nvs_flash.h"
#include "sensor_rtc.h"
#include "ui.h"

static const char *TAG = "app";

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(sensor_rtc_init());
    ESP_ERROR_CHECK(can_cfg_init());

    esp_console_config_t console_cfg = {
        .max_cmdline_length = 256,
        .max_cmdline_args = 16,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
        .hint_color = atoi(LOG_COLOR_CYAN),
#endif
    };
    ESP_ERROR_CHECK(esp_console_init(&console_cfg));
    linenoiseSetMultiLine(true);
    linenoiseHistorySetMaxLen(20);
    ESP_ERROR_CHECK(can_console_register());

    ESP_ERROR_CHECK(display_init());
    ESP_ERROR_CHECK(ui_init());
    ESP_ERROR_CHECK(can_service_start());

    ESP_LOGI(TAG, "System started. Use 'cancfg show' / 'cancfg set ...' / 'cancfg save' on USB JTAG console.");
}
