# Components

## Main runtime component

See:

- `/home/runner/work/hpe-gauge-small/hpe-gauge-small/main/README.md`
- `/home/runner/work/hpe-gauge-small/hpe-gauge-small/main/include/README.md`

Key runtime units:

- `display.c`: LCD/touch/LVGL init.
- `ui.c`: gauge and status pages.
- `can_cfg.c` + `can_console.c`: CAN profile persistence and console management.
- `can_service.c`: CAN frame decode and telemetry values.
- `sensor_rtc.c`: IMU/RTC probing.
- `web_ui.c`: WiFi AP + mobile web UI + WebSocket updates.
