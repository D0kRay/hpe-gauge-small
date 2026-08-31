#include "raycaster_game.h"

#include <cstring>

#include "game/game.hpp"
#include "game/render/Screen.hpp"

namespace {
bool s_initialized = false;

void to_game_input(const raycaster_input_t *input, InputData *game_input)
{
    if (!input || !game_input) {
        return;
    }

    *game_input = InputData{};
    game_input->correct = input->touched || input->fire || input->forward || input->backward ||
                          input->turn_left || input->turn_right;
    game_input->x = static_cast<int16_t>(input->turn_delta_x);
    game_input->flags = 0;

    if (input->turn_left) {
        game_input->flags |= (1U << LEFT);
    }
    if (input->turn_right) {
        game_input->flags |= (1U << RIGHT);
    }
    if (input->forward) {
        game_input->flags |= (1U << UP);
    }
    if (input->backward) {
        game_input->flags |= (1U << DOWN);
    }
    if (input->fire) {
        game_input->flags |= (1U << 7);
    }
}
} // namespace

extern "C" void raycaster_game_init(void)
{
    if (!s_initialized) {
        Game::setup();
        s_initialized = true;
    }
}

extern "C" void raycaster_game_reset(void)
{
    InputData reset_input = InputData{};
    Game::reset(&reset_input);
}

extern "C" bool raycaster_game_step(const raycaster_input_t *input, uint16_t *out_frame, size_t out_frame_size)
{
    if (!s_initialized) {
        raycaster_game_init();
    }

    Screen::init();

    InputData game_input = InputData{};
    to_game_input(input, &game_input);

    const float dt = 1.0f / 30.0f;
    const bool should_continue = Game::loop(dt, game_input);

    if (out_frame && out_frame_size >= Screen::SCREEN_SIZE) {
        if (Screen::_screen != nullptr) {
            std::memcpy(out_frame, Screen::_screen, Screen::SCREEN_SIZE * sizeof(uint16_t));
        }
    }

    return should_continue;
}
