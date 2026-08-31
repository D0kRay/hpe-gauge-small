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
#include "web_ui.h"

static const char *TAG = "app";

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = sensor_rtc_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "sensor/rtc init failed, continuing: %s", esp_err_to_name(ret));
    }
    ESP_ERROR_CHECK(can_cfg_init());

    linenoiseSetMultiLine(true);
    linenoiseHistorySetMaxLen(20);
    ESP_ERROR_CHECK(can_console_register());
    ESP_ERROR_CHECK(esp_console_register_help_command());

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.max_history_len = 20;
    repl_cfg.prompt = "esp> ";

    esp_console_dev_usb_serial_jtag_config_t dev_cfg = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&dev_cfg, &repl_cfg, &repl));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    ESP_ERROR_CHECK(display_init());
    ESP_ERROR_CHECK(ui_init());
    ESP_ERROR_CHECK(display_start());
    ret = can_service_start();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "TWAI init failed, continuing without CAN RX: %s", esp_err_to_name(ret));
    }
    ret = web_ui_start();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "web UI start failed, continuing without web dashboard: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "System started. Use 'cancfg show' / 'cancfg set <name> <id> <start_bit> <bit_len> <le|be> <signed> <factor> <offset>' / 'cancfg save'.");
}
