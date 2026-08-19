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

static void can_cfg_set_defaults(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.signal_count = 2;

    strncpy(s_cfg.signals[0].name, "speed", CAN_SIGNAL_NAME_MAX - 1);
    s_cfg.signals[0].can_id = 0x100;
    s_cfg.signals[0].start_byte = 0;
    s_cfg.signals[0].length_bytes = 2;
    s_cfg.signals[0].scale = 0.01f;

    strncpy(s_cfg.signals[1].name, "rpm", CAN_SIGNAL_NAME_MAX - 1);
    s_cfg.signals[1].can_id = 0x101;
    s_cfg.signals[1].start_byte = 0;
    s_cfg.signals[1].length_bytes = 2;
    s_cfg.signals[1].scale = 1.0f;
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
        unsigned int start_byte;
        unsigned int length_bytes;
        if (sscanf(line, "%15[^,],%x,%u,%u,%f,%f",
                   sig->name,
                   &can_id,
                   &start_byte,
                   &length_bytes,
                   &sig->scale,
                   &sig->offset) == 6) {
            sig->can_id = can_id;
            sig->start_byte = start_byte;
            sig->length_bytes = length_bytes;
            parsed.signal_count++;
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

    fprintf(f, "#name,can_id_hex,start_byte,length_bytes,scale,offset\n");
    for (uint8_t i = 0; i < s_cfg.signal_count; ++i) {
        const can_signal_cfg_t *sig = &s_cfg.signals[i];
        fprintf(f, "%s,%03x,%u,%u,%.5f,%.5f\n",
                sig->name,
                sig->can_id,
                sig->start_byte,
                sig->length_bytes,
                (double)sig->scale,
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
