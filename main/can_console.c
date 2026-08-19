#include "can_console.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "can_cfg.h"
#include "esp_console.h"
#include "esp_log.h"

static const char *TAG = "can_console";

static int cancfg_cmd(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: cancfg show | cancfg save | cancfg set <name> <can_id_hex> <start_byte> <length_bytes> <scale> <offset>\n");
        return 1;
    }

    if (strcmp(argv[1], "show") == 0) {
        const can_cfg_t *cfg = can_cfg_get();
        for (uint8_t i = 0; i < cfg->signal_count; ++i) {
            const can_signal_cfg_t *s = &cfg->signals[i];
            printf("%s: id=0x%03lx start=%u len=%u scale=%.3f offset=%.3f\n",
                   s->name,
                   (unsigned long)s->can_id,
                   s->start_byte,
                   s->length_bytes,
                   (double)s->scale,
                   (double)s->offset);
        }
        return 0;
    }

    if (strcmp(argv[1], "save") == 0) {
        return can_cfg_save() == ESP_OK ? 0 : 1;
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc != 8) {
            printf("Usage: cancfg set <name> <can_id_hex> <start_byte> <length_bytes> <scale> <offset>\n");
            return 1;
        }

        can_signal_cfg_t sig = {0};
        strncpy(sig.name, argv[2], CAN_SIGNAL_NAME_MAX - 1);
        sig.can_id = strtoul(argv[3], NULL, 16);
        sig.start_byte = (uint8_t)strtoul(argv[4], NULL, 10);
        sig.length_bytes = (uint8_t)strtoul(argv[5], NULL, 10);
        sig.scale = strtof(argv[6], NULL);
        sig.offset = strtof(argv[7], NULL);

        if (can_cfg_set(&sig) != ESP_OK) {
            ESP_LOGE(TAG, "cannot set signal");
            return 1;
        }

        return 0;
    }

    return 1;
}

esp_err_t can_console_register(void)
{
    const esp_console_cmd_t cmd = {
        .command = "cancfg",
        .help = "show/save/set CAN signal mapping in littlefs",
        .hint = NULL,
        .func = &cancfg_cmd,
    };

    return esp_console_cmd_register(&cmd);
}
