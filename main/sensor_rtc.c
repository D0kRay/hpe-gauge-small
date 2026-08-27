#include "sensor_rtc.h"

#include <string.h>
#include "bsp.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"

#define QMI8658_ADDR_PRIMARY 0x6B
#define QMI8658_ADDR_ALT 0x6A
#define PCF85063_ADDR 0x51

#define QMI8658_REG_WHOAMI 0x00
#define QMI8658_WHOAMI_EXPECTED 0x05
#define QMI8658_REG_CTRL2 0x03
#define QMI8658_REG_CTRL3 0x04
#define QMI8658_REG_CTRL5 0x06
#define QMI8658_REG_CTRL7 0x08

#define PCF85063_REG_CTRL1 0x00
#define PCF85063_REG_CTRL2 0x01

static const char *TAG = "sensor_rtc";
static sensor_rtc_status_t s_status;

static esp_err_t i2c_read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(dev, &reg, sizeof(reg), data, len, 100);
}

static esp_err_t i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(dev, buf, sizeof(buf), 100);
}

esp_err_t sensor_rtc_init(void)
{
    const bsp_config_t *bsp = bsp_config_get();
    i2c_master_bus_handle_t bus = NULL;
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = bsp->i2c_port,
        .sda_io_num = bsp->pin_i2c_sda,
        .scl_io_num = bsp->pin_i2c_scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &bus), TAG, "i2c master bus init failed");

    memset(&s_status, 0, sizeof(s_status));

    i2c_master_dev_handle_t qmi_dev = NULL;
    uint8_t qmi_addr_in_use = 0;
    const uint8_t qmi_candidate_addrs[] = {QMI8658_ADDR_PRIMARY, QMI8658_ADDR_ALT};
    for (size_t i = 0; i < (sizeof(qmi_candidate_addrs) / sizeof(qmi_candidate_addrs[0])); ++i) {
        i2c_device_config_t qmi_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = qmi_candidate_addrs[i],
            .scl_speed_hz = 400000,
        };
        if (i2c_master_bus_add_device(bus, &qmi_cfg, &qmi_dev) != ESP_OK) {
            continue;
        }

        uint8_t whoami = 0;
        if (i2c_read_reg(qmi_dev, QMI8658_REG_WHOAMI, &whoami, 1) == ESP_OK) {
            qmi_addr_in_use = qmi_candidate_addrs[i];
            s_status.imu_detected = true;
            s_status.imu_whoami = whoami;
            break;
        }

        i2c_master_bus_rm_device(qmi_dev);
        qmi_dev = NULL;
    }

    i2c_device_config_t rtc_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCF85063_ADDR,
        .scl_speed_hz = 400000,
    };
    i2c_master_dev_handle_t rtc_dev = NULL;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &rtc_cfg, &rtc_dev), TAG, "PCF85063 i2c add failed");

    if (s_status.imu_detected) {
        ESP_LOGI(TAG, "QMI8658 detected @ 0x%02X WHO_AM_I: 0x%02X", qmi_addr_in_use, s_status.imu_whoami);
        if (s_status.imu_whoami == QMI8658_WHOAMI_EXPECTED) {
            (void)i2c_write_reg(qmi_dev, QMI8658_REG_CTRL2, 0x23);
            (void)i2c_write_reg(qmi_dev, QMI8658_REG_CTRL3, 0x43);
            (void)i2c_write_reg(qmi_dev, QMI8658_REG_CTRL5, 0x00);
            (void)i2c_write_reg(qmi_dev, QMI8658_REG_CTRL7, 0x03);
        } else {
            ESP_LOGW(TAG, "QMI8658 WHO_AM_I mismatch (expected 0x%02X)", QMI8658_WHOAMI_EXPECTED);
        }
    } else {
        ESP_LOGW(TAG, "QMI8658 not detected");
    }

    uint8_t rtc_ctrl = 0;
    if (i2c_read_reg(rtc_dev, PCF85063_REG_CTRL1, &rtc_ctrl, 1) == ESP_OK) {
        s_status.rtc_detected = true;
        s_status.rtc_ctrl1 = rtc_ctrl;
        ESP_LOGI(TAG, "PCF85063 CTRL1: 0x%02X", rtc_ctrl);
        (void)i2c_write_reg(rtc_dev, PCF85063_REG_CTRL2, 0x20);
    } else {
        ESP_LOGW(TAG, "PCF85063 not detected");
    }

    i2c_master_bus_rm_device(rtc_dev);
    if (qmi_dev) {
        i2c_master_bus_rm_device(qmi_dev);
    }
    i2c_del_master_bus(bus);

    return ESP_OK;
}

sensor_rtc_status_t sensor_rtc_get_status(void)
{
    return s_status;
}
