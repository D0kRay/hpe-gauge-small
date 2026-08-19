# main component

This is the primary ESP-IDF application component. It contains startup logic, hardware abstraction wiring, UI rendering, CAN decoding, and sensor probing.

## File responsibilities

- `app_main.c`: boot sequence, NVS init, console setup, and service startup orchestration.
- `bsp.c`: centralized board/pin/runtime defaults sourced from Kconfig.
- `display.c`: GC9A01 panel + CST816 touch bring-up and LVGL runtime/task integration.
- `ui.c`: LVGL tabbed UI orchestration and cross-screen refresh hooks.
- `ui_widget_dual_gauge.c`: custom LVGL C-class dual-arc gauge widget.
- `ui_widget_info_panel.c`: custom LVGL C-class information panel widget.
- `ui_screen_gauge.c`: gauge screen composition.
- `ui_screen_can.c`: CAN screen composition.
- `ui_screen_sensors.c`: sensor screen composition.
- `ui_screen_ota.c`: OTA status screen composition.
- `can_cfg.c`: LittleFS-backed CAN signal profile loading, validation, and persistence.
- `can_console.c`: `cancfg` shell command for show/set/save profile operations.
- `can_service.c`: TWAI driver startup and frame parsing into speed/RPM values.
- `sensor_rtc.c`: I2C initialization and QMI8658/PCF85063 detection status probes.
- `web_ui.c`: WiFi AP startup plus mobile HTTP/WebSocket telemetry page and signed OTA upload endpoint.

## Public API

Headers exported by this component are in `main/include/` and documented in `main/include/README.md`.
