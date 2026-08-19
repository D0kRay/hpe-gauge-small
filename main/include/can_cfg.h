#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define CAN_CFG_MAX_SIGNALS 8
#define CAN_SIGNAL_NAME_MAX 16

typedef struct {
    char name[CAN_SIGNAL_NAME_MAX];
    uint32_t can_id;
    uint16_t start_bit;
    uint8_t bit_length;
    bool is_little_endian;
    bool is_signed;
    float factor;
    float offset;
} can_signal_cfg_t;

typedef struct {
    can_signal_cfg_t signals[CAN_CFG_MAX_SIGNALS];
    uint8_t signal_count;
} can_cfg_t;

esp_err_t can_cfg_init(void);
const can_cfg_t *can_cfg_get(void);
esp_err_t can_cfg_set(const can_signal_cfg_t *signal);
esp_err_t can_cfg_save(void);
esp_err_t can_cfg_load(void);
bool can_cfg_get_signal(const char *name, can_signal_cfg_t *out_signal);
