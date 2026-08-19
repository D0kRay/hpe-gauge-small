#include "sensor_rtc.h"

#include <string.h>
#include "bsp.h"
#include "driver/i2c.h"
#include "esp_check.h"
#include "esp_log.h"

#define QMI8658_ADDR 0x6B
#define PCF85063_ADDR 0x51

static const char *TAG = "sensor_rtc";
static sensor_rtc_status_t s_status;

static esp_err_t i2c_read_reg(i2c_port_t port, uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    ESP_RETURN_ON_FALSE(cmd != NULL, ESP_ERR_NO_MEM, TAG, "i2c cmd alloc failed");
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, &data[len - 1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t sensor_rtc_init(void)
{
    const bsp_config_t *bsp = bsp_config_get();

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = bsp->pin_i2c_sda,
        .scl_io_num = bsp->pin_i2c_scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };

    ESP_RETURN_ON_ERROR(i2c_param_config(bsp->i2c_port, &conf), TAG, "i2c param config failed");
    ESP_RETURN_ON_ERROR(i2c_driver_install(bsp->i2c_port, conf.mode, 0, 0, 0), TAG, "i2c driver install failed");

    memset(&s_status, 0, sizeof(s_status));

    uint8_t qmi_whoami = 0;
    if (i2c_read_reg(bsp->i2c_port, QMI8658_ADDR, 0x00, &qmi_whoami, 1) == ESP_OK) {
        s_status.imu_detected = true;
        s_status.imu_whoami = qmi_whoami;
        ESP_LOGI(TAG, "QMI8658 WHO_AM_I: 0x%02X", qmi_whoami);
    } else {
        ESP_LOGW(TAG, "QMI8658 not detected");
    }

    uint8_t rtc_ctrl = 0;
    if (i2c_read_reg(bsp->i2c_port, PCF85063_ADDR, 0x00, &rtc_ctrl, 1) == ESP_OK) {
        s_status.rtc_detected = true;
        s_status.rtc_ctrl1 = rtc_ctrl;
        ESP_LOGI(TAG, "PCF85063 CTRL1: 0x%02X", rtc_ctrl);
    } else {
        ESP_LOGW(TAG, "PCF85063 not detected");
    }

    return ESP_OK;
}

sensor_rtc_status_t sensor_rtc_get_status(void)
{
    return s_status;
}
