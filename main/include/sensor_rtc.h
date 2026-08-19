#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Snapshot of detected IMU/RTC presence and probe register values.
 */
typedef struct {
    bool imu_detected;
    uint8_t imu_whoami;
    bool rtc_detected;
    uint8_t rtc_ctrl1;
} sensor_rtc_status_t;

/**
 * @brief Initialize I2C and probe IMU/RTC devices for status reporting.
 *
 * @return ESP_OK on success or an ESP_ERR_* code on I2C initialization failures.
 */
esp_err_t sensor_rtc_init(void);

/**
 * @brief Get the latest probed IMU/RTC status snapshot.
 *
 * @return Current sensor and RTC status data.
 */
sensor_rtc_status_t sensor_rtc_get_status(void);
