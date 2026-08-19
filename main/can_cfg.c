#include "can_cfg.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "esp_check.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "can_cfg";
static can_cfg_t s_cfg;

static bool can_cfg_validate_signal(const can_signal_cfg_t *signal)
{
    if (!signal || signal->bit_length == 0 || signal->bit_length > 32 || signal->start_bit >= 64) {
        return false;
    }
    if (signal->is_little_endian) {
        if ((signal->start_bit + signal->bit_length) > 64) {
            return false;
        }
    } else {
        int bit = signal->start_bit;
        for (uint8_t i = 0; i < signal->bit_length; ++i) {
            if (bit < 0 || bit >= 64) {
                return false;
            }
            bit = (bit % 8 == 0) ? (bit + 15) : (bit - 1);
        }
    }
    return true;
}

static void can_cfg_set_defaults(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.signal_count = 2;

    strncpy(s_cfg.signals[0].name, "speed", CAN_SIGNAL_NAME_MAX - 1);
    s_cfg.signals[0].can_id = 0x100;
    s_cfg.signals[0].start_bit = 0;
    s_cfg.signals[0].bit_length = 16;
    s_cfg.signals[0].is_little_endian = true;
    s_cfg.signals[0].is_signed = false;
    s_cfg.signals[0].factor = 0.01f;
    s_cfg.signals[0].offset = 0.0f;

    strncpy(s_cfg.signals[1].name, "rpm", CAN_SIGNAL_NAME_MAX - 1);
    s_cfg.signals[1].can_id = 0x101;
    s_cfg.signals[1].start_bit = 0;
    s_cfg.signals[1].bit_length = 16;
    s_cfg.signals[1].is_little_endian = true;
    s_cfg.signals[1].is_signed = false;
    s_cfg.signals[1].factor = 1.0f;
    s_cfg.signals[1].offset = 0.0f;
}

esp_err_t can_cfg_load(void)
{
    FILE *f = fopen(CONFIG_HPE_CAN_CFG_PATH, "r");
    if (!f) {
        ESP_LOGW(TAG, "CAN cfg missing, writing defaults");
        can_cfg_set_defaults();
        return can_cfg_save();
    }

    can_cfg_t parsed = {0};
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        if (parsed.signal_count >= CAN_CFG_MAX_SIGNALS) {
            break;
        }

        can_signal_cfg_t *sig = &parsed.signals[parsed.signal_count];
        unsigned int can_id;
        unsigned int start_bit;
        unsigned int bit_length;
        unsigned int is_signed;
        char endian[4] = {0};
        if (sscanf(line, "%15[^,],%x,%u,%u,%3[^,],%u,%f,%f",
                   sig->name,
                   &can_id,
                   &start_bit,
                   &bit_length,
                   endian,
                   &is_signed,
                   &sig->factor,
                   &sig->offset) == 8) {
            sig->can_id = can_id;
            sig->start_bit = start_bit;
            sig->bit_length = bit_length;
            if (strcmp(endian, "le") == 0) {
                sig->is_little_endian = true;
            } else if (strcmp(endian, "be") == 0) {
                sig->is_little_endian = false;
            } else {
                continue;
            }
            sig->is_signed = (is_signed != 0);
            if (can_cfg_validate_signal(sig)) {
                parsed.signal_count++;
            }
        }
    }

    fclose(f);
    if (parsed.signal_count == 0) {
        can_cfg_set_defaults();
        return can_cfg_save();
    }

    s_cfg = parsed;
    return ESP_OK;
}

esp_err_t can_cfg_save(void)
{
    FILE *f = fopen(CONFIG_HPE_CAN_CFG_PATH, "w");
    ESP_RETURN_ON_FALSE(f != NULL, ESP_FAIL, TAG, "open %s failed", CONFIG_HPE_CAN_CFG_PATH);

    fprintf(f, "#name,can_id_hex,start_bit,bit_length,endian,signed,factor,offset\n");
    for (uint8_t i = 0; i < s_cfg.signal_count; ++i) {
        const can_signal_cfg_t *sig = &s_cfg.signals[i];
        fprintf(f, "%s,%03x,%u,%u,%s,%u,%.5f,%.5f\n",
                sig->name,
                sig->can_id,
                sig->start_bit,
                sig->bit_length,
                sig->is_little_endian ? "le" : "be",
                sig->is_signed ? 1U : 0U,
                (double)sig->factor,
                (double)sig->offset);
    }

    fclose(f);
    return ESP_OK;
}

esp_err_t can_cfg_init(void)
{
    const esp_vfs_littlefs_conf_t cfg = {
        .base_path = "/littlefs",
        .partition_label = "littlefs",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
    ESP_RETURN_ON_ERROR(esp_vfs_littlefs_register(&cfg), TAG, "littlefs mount failed");

    return can_cfg_load();
}

const can_cfg_t *can_cfg_get(void)
{
    return &s_cfg;
}

bool can_cfg_get_signal(const char *name, can_signal_cfg_t *out_signal)
{
    for (uint8_t i = 0; i < s_cfg.signal_count; ++i) {
        if (strncmp(s_cfg.signals[i].name, name, CAN_SIGNAL_NAME_MAX) == 0) {
            *out_signal = s_cfg.signals[i];
            return true;
        }
    }
    return false;
}

esp_err_t can_cfg_set(const can_signal_cfg_t *signal)
{
    if (!can_cfg_validate_signal(signal)) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint8_t i = 0; i < s_cfg.signal_count; ++i) {
        if (strncmp(s_cfg.signals[i].name, signal->name, CAN_SIGNAL_NAME_MAX) == 0) {
            s_cfg.signals[i] = *signal;
            return ESP_OK;
        }
    }

    if (s_cfg.signal_count >= CAN_CFG_MAX_SIGNALS) {
        return ESP_ERR_NO_MEM;
    }

    s_cfg.signals[s_cfg.signal_count++] = *signal;
    return ESP_OK;
}
