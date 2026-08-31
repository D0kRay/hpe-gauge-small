# Raycaster game

This project includes a compact raycaster demo adapted for the ESP32-S3 gauge display. It renders a pseudo-3D dungeon view into a LVGL canvas and uses the touch panel as the main game controller.

## Controls

- Left side: turn left
- Right side: turn right
- Upper half: move forward
- Lower half: move backward
- Center press: fire

The menu overlay remains disabled while the raycaster screen is active so touch gestures are not captured by the UI navigation menu during gameplay.

## Integration notes

- The game is initialized through `raycaster_game_init()` and stepped through `raycaster_game_step()`.
- The rendered frame is written into a 240 x 160 RGB565 buffer to match the display canvas.
- The screen is exposed through the UI switch menu as `raycaster` and can also be selected with the console command:

  `screen raycaster`

## Build / run

The game is built as part of the main firmware target and is launched from the UI when the raycaster screen is selected.

## Notes

This is a lightweight gameplay demo intended for the embedded display and touch hardware. It uses a minimal control scheme to suit the round 240 x 240 screen and touch-only input model.
