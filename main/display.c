#include "display.h"

#include <string.h>
#include "bsp.h"
#include "esp_check.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "lvgl.h"

static const char *TAG = "display";

static SemaphoreHandle_t s_lvgl_mutex;
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_touch_handle_t s_touch;
static lv_display_t *s_lv_disp;
static lv_indev_t *s_lv_indev;
static lv_color_t *s_buf1;
static lv_color_t *s_buf2;

static void lv_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(CONFIG_HPE_LVGL_TICK_MS);
}

static void lvgl_task(void *arg)
{
    (void)arg;
    while (true) {
        display_lvgl_lock();
        lv_timer_handler();
        display_lvgl_unlock();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    (void)disp;
    esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    lv_display_flush_ready(disp);
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    uint16_t x[1] = {0};
    uint16_t y[1] = {0};
    uint16_t strength[1] = {0};
    uint8_t points = 0;

    esp_lcd_touch_read_data(s_touch);
    bool touched = esp_lcd_touch_get_coordinates(s_touch, x, y, strength, &points, 1);
    if (touched && points > 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = (int16_t)x[0];
        data->point.y = (int16_t)y[0];
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void display_lvgl_lock(void)
{
    xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);
}

void display_lvgl_unlock(void)
{
    xSemaphoreGive(s_lvgl_mutex);
}

esp_err_t display_init(void)
{
    const bsp_config_t *bsp = bsp_config_get();
    esp_err_t ret;

    s_lvgl_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_lvgl_mutex != NULL, ESP_ERR_NO_MEM, TAG, "LVGL mutex create failed");

    spi_bus_config_t buscfg = {
        .sclk_io_num = bsp->pin_lcd_sclk,
        .mosi_io_num = bsp->pin_lcd_mosi,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = 240 * 80 * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(bsp->lcd_spi_host, &buscfg, SPI_DMA_CH_AUTO), TAG, "spi_bus_initialize failed");

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = bsp->pin_lcd_dc,
        .cs_gpio_num = bsp->pin_lcd_cs,
        .pclk_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)bsp->lcd_spi_host, &io_config, &io_handle), TAG, "new panel io failed");

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = bsp->pin_lcd_rst,
        .bits_per_pixel = 16,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &s_panel), TAG, "new gc9a01 panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, true), TAG, "panel invert failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, true, false), TAG, "panel mirror failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "panel on failed");

    gpio_config_t bl_cfg = {
        .pin_bit_mask = 1ULL << bsp->pin_lcd_bl,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&bl_cfg), TAG, "backlight gpio config failed");
    gpio_set_level(bsp->pin_lcd_bl, 1);

    lv_init();
    s_buf1 = heap_caps_malloc(240 * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s_buf2 = heap_caps_malloc(240 * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_RETURN_ON_FALSE(s_buf1 && s_buf2, ESP_ERR_NO_MEM, TAG, "lvgl buffers alloc failed");

    s_lv_disp = lv_display_create(240, 240);
    lv_display_set_buffers(s_lv_disp, s_buf1, s_buf2, 240 * 40 * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_lv_disp, flush_cb);

    i2c_master_bus_handle_t i2c_bus_handle = NULL;
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = bsp->i2c_port,
        .sda_io_num = bsp->pin_i2c_sda,
        .scl_io_num = bsp->pin_i2c_scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle), TAG, "i2c master bus init failed");

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_bus_handle, &tp_io_config, &tp_io_handle), TAG, "new touch panel io failed");

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = 240,
        .y_max = 240,
        .rst_gpio_num = bsp->pin_touch_rst,
        .int_gpio_num = bsp->pin_touch_int,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    ret = esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &s_touch);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "touch init failed: %s", esp_err_to_name(ret));
    } else {
        s_lv_indev = lv_indev_create();
        lv_indev_set_type(s_lv_indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(s_lv_indev, touch_read_cb);
    }

    const esp_timer_create_args_t tick_args = {
        .callback = lv_tick_cb,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer;
    ESP_RETURN_ON_ERROR(esp_timer_create(&tick_args, &tick_timer), TAG, "lv tick create failed");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(tick_timer, CONFIG_HPE_LVGL_TICK_MS * 1000), TAG, "lv tick start failed");

    ESP_RETURN_ON_FALSE(xTaskCreate(lvgl_task, "lvgl", 4096, NULL, 5, NULL) == pdPASS, ESP_ERR_NO_MEM, TAG, "lv task create failed");

    return ESP_OK;
}
