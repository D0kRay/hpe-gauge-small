# main/include API reference

These headers define the public interfaces used across the application component.

## Modules

- `bsp.h`: board-level pin/peripheral configuration accessor.
- `display.h`: display/touch/LVGL initialization and UI locking primitives.
- `ui.h`: LVGL screen construction and gauge update API.
- `can_cfg.h`: CAN signal mapping model plus load/save/query/update operations.
- `can_console.h`: console command registration for CAN profile management.
- `can_service.h`: TWAI receive service startup entrypoint.
- `sensor_rtc.h`: IMU/RTC probe initialization and status access.
- `web_ui.h`: WiFi AP + HTTP/WebSocket UI startup entrypoint.

## Notes for developers

- Keep function-level descriptions in each header up to date when signatures or behavior changes.
- Prefer adding new cross-module APIs in headers and keeping file-local helpers `static` in `.c` files.
