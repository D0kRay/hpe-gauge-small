#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t turn_delta_x;
    bool touched;
    bool forward;
    bool backward;
    bool turn_left;
    bool turn_right;
    bool fire;
} raycaster_input_t;

void raycaster_game_init(void);
void raycaster_game_reset(void);
bool raycaster_game_step(const raycaster_input_t *input, uint16_t *out_frame, size_t out_frame_size);

#ifdef __cplusplus
}
#endif
