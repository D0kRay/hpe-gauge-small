#pragma once

#include <stdint.h>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"

/**
 * @brief Board support package pin and peripheral configuration.
 */
typedef struct {
    spi_host_device_t lcd_spi_host;
    gpio_num_t pin_lcd_mosi;
    gpio_num_t pin_lcd_sclk;
    gpio_num_t pin_lcd_cs;
    gpio_num_t pin_lcd_dc;
    gpio_num_t pin_lcd_rst;
    gpio_num_t pin_lcd_bl;

    i2c_port_t i2c_port;
    gpio_num_t pin_i2c_sda;
    gpio_num_t pin_i2c_scl;
    gpio_num_t pin_touch_int;
    gpio_num_t pin_touch_rst;

    gpio_num_t pin_twai_tx;
    gpio_num_t pin_twai_rx;
    int twai_bitrate;
} bsp_config_t;

/**
 * @brief Get the immutable BSP configuration derived from Kconfig defaults.
 *
 * @return Pointer to the global configuration instance.
 */
const bsp_config_t *bsp_config_get(void);
