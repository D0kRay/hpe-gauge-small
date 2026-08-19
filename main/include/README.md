# main/include API reference

These headers define the public interfaces used across the application component.

## Modules

- `bsp.h`: board-level pin/peripheral configuration accessor.
- `display.h`: display/touch/LVGL initialization and UI locking primitives.
- `ui.h`: LVGL screen construction and runtime update APIs (gauge + OTA status).
- `ui_screen_gauge.h`: Gauge tab screen abstraction.
- `ui_screen_can.h`: CAN tab screen abstraction.
- `ui_screen_sensors.h`: Sensors tab screen abstraction.
- `ui_screen_ota.h`: OTA tab screen abstraction.
- `ui_widget_dual_gauge.h`: Custom LVGL C-class dual-gauge widget.
- `ui_widget_info_panel.h`: Custom LVGL C-class info panel widget.
- `can_cfg.h`: CAN signal mapping model plus load/save/query/update operations.
- `can_console.h`: console command registration for CAN profile management.
- `can_service.h`: TWAI receive service startup entrypoint.
- `sensor_rtc.h`: IMU/RTC probe initialization and status access.
- `web_ui.h`: WiFi AP + HTTP/WebSocket UI startup entrypoint.

## Notes for developers

- Keep function-level descriptions in each header up to date when signatures or behavior changes.
- Prefer adding new cross-module APIs in headers and keeping file-local helpers `static` in `.c` files.
