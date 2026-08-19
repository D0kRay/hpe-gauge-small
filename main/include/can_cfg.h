#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define CAN_CFG_MAX_SIGNALS 8
#define CAN_SIGNAL_NAME_MAX 16

/**
 * @brief Runtime CAN signal mapping entry compatible with DBC-like encoding.
 */
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

/**
 * @brief In-memory CAN mapping table persisted on LittleFS.
 */
typedef struct {
    can_signal_cfg_t signals[CAN_CFG_MAX_SIGNALS];
    uint8_t signal_count;
} can_cfg_t;

/**
 * @brief Mount LittleFS and load CAN signal configuration into memory.
 *
 * @return ESP_OK on success or an ESP_ERR_* code on failure.
 */
esp_err_t can_cfg_init(void);

/**
 * @brief Get the current in-memory CAN signal configuration.
 *
 * @return Pointer to the active configuration.
 */
const can_cfg_t *can_cfg_get(void);

/**
 * @brief Add or update a CAN signal mapping entry.
 *
 * @param signal Signal definition to add or replace.
 * @return ESP_OK on success or an ESP_ERR_* code on validation/capacity errors.
 */
esp_err_t can_cfg_set(const can_signal_cfg_t *signal);

/**
 * @brief Persist the current in-memory CAN mapping to LittleFS.
 *
 * @return ESP_OK on success or an ESP_ERR_* code on failure.
 */
esp_err_t can_cfg_save(void);

/**
 * @brief Reload CAN mapping from LittleFS, falling back to defaults when missing/invalid.
 *
 * @return ESP_OK on success or an ESP_ERR_* code on file access errors.
 */
esp_err_t can_cfg_load(void);

/**
 * @brief Look up a CAN signal mapping by name.
 *
 * @param name Signal name to search for.
 * @param out_signal Output signal copy when found.
 * @return true if the signal exists, false otherwise.
 */
bool can_cfg_get_signal(const char *name, can_signal_cfg_t *out_signal);
