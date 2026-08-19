#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    bool imu_detected;
    uint8_t imu_whoami;
    bool rtc_detected;
    uint8_t rtc_ctrl1;
} sensor_rtc_status_t;

esp_err_t sensor_rtc_init(void);
sensor_rtc_status_t sensor_rtc_get_status(void);
