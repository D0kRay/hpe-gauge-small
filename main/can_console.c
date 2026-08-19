#include "can_console.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "can_cfg.h"
#include "esp_console.h"
#include "esp_log.h"

static const char *TAG = "can_console";

static int cancfg_cmd(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: cancfg show | cancfg save | cancfg set <name> <can_id_hex> <start_bit> <bit_length> <endian:le|be> <signed:0|1> <factor> <offset>\n");
        return 1;
    }

    if (strcmp(argv[1], "show") == 0) {
        const can_cfg_t *cfg = can_cfg_get();
        for (uint8_t i = 0; i < cfg->signal_count; ++i) {
            const can_signal_cfg_t *s = &cfg->signals[i];
            printf("%s: id=0x%03lx start_bit=%u bit_len=%u endian=%s signed=%u factor=%.3f offset=%.3f\n",
                   s->name,
                   (unsigned long)s->can_id,
                   s->start_bit,
                   s->bit_length,
                   s->is_little_endian ? "le" : "be",
                   s->is_signed ? 1U : 0U,
                   (double)s->factor,
                   (double)s->offset);
        }
        return 0;
    }

    if (strcmp(argv[1], "save") == 0) {
        return can_cfg_save() == ESP_OK ? 0 : 1;
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc != 10) {
            printf("Usage: cancfg set <name> <can_id_hex> <start_bit> <bit_length> <endian:le|be> <signed:0|1> <factor> <offset>\n");
            return 1;
        }

        can_signal_cfg_t sig = {0};
        strncpy(sig.name, argv[2], CAN_SIGNAL_NAME_MAX - 1);
        sig.can_id = strtoul(argv[3], NULL, 16);
        sig.start_bit = (uint16_t)strtoul(argv[4], NULL, 10);
        sig.bit_length = (uint8_t)strtoul(argv[5], NULL, 10);
        if (strcmp(argv[6], "le") == 0) {
            sig.is_little_endian = true;
        } else if (strcmp(argv[6], "be") == 0) {
            sig.is_little_endian = false;
        } else {
            printf("endian must be 'le' or 'be'\n");
            return 1;
        }
        sig.is_signed = (strtoul(argv[7], NULL, 10) != 0);
        sig.factor = strtof(argv[8], NULL);
        sig.offset = strtof(argv[9], NULL);

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
