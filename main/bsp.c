#include "bsp.h"
#include "sdkconfig.h"

static const bsp_config_t s_cfg = {
    .lcd_spi_host = (spi_host_device_t)CONFIG_HPE_LCD_HOST,
    .pin_lcd_mosi = CONFIG_HPE_PIN_LCD_MOSI,
    .pin_lcd_sclk = CONFIG_HPE_PIN_LCD_SCLK,
    .pin_lcd_cs = CONFIG_HPE_PIN_LCD_CS,
    .pin_lcd_dc = CONFIG_HPE_PIN_LCD_DC,
    .pin_lcd_rst = CONFIG_HPE_PIN_LCD_RST,
    .pin_lcd_bl = CONFIG_HPE_PIN_LCD_BL,
    .i2c_port = I2C_NUM_0,
    .pin_i2c_sda = CONFIG_HPE_PIN_TOUCH_SDA,
    .pin_i2c_scl = CONFIG_HPE_PIN_TOUCH_SCL,
    .pin_touch_int = CONFIG_HPE_PIN_TOUCH_INT,
    .pin_touch_rst = CONFIG_HPE_PIN_TOUCH_RST,
    .pin_twai_tx = CONFIG_HPE_PIN_TWAI_TX,
    .pin_twai_rx = CONFIG_HPE_PIN_TWAI_RX,
    .twai_bitrate = CONFIG_HPE_TWAI_BITRATE,
};

const bsp_config_t *bsp_config_get(void)
{
    return &s_cfg;
}
