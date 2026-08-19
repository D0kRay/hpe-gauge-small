# hpe-gauge-small

[![Issues](https://img.shields.io/github/issues/D0kRay/hpe-gauge-small)](https://github.com/D0kRay/hpe-gauge-small/issues)
[![Latest Release](https://img.shields.io/github/v/release/D0kRay/hpe-gauge-small)](https://github.com/D0kRay/hpe-gauge-small/releases/latest)
[![Build Status](https://github.com/D0kRay/hpe-gauge-small/actions/workflows/build-idf.yml/badge.svg)](https://github.com/D0kRay/hpe-gauge-small/actions/workflows/build-idf.yml)

ESP32-S3 ESP-IDF project for a 240x240 round gauge with:

- GC9A01 SPI display
- CST816 touch controller
- LVGL 9 UI (modular per-screen files with custom C-class widgets):
  - Fancy speed + RPM round gauge page
  - CAN profile page
  - IMU/RTC status page
  - OTA status page
- TWAI/CAN interface (for MCP transceiver)
- USB Serial/JTAG console
- QMI8658 IMU and PCF85063 RTC presence checks
- LittleFS-backed DBC-style CAN signal mapping (`/littlefs/can_messages.cfg`)
- WiFi AP mode with mobile web page, WebSocket live telemetry, and signed-image OTA upload

## Managed components

Configured in `/home/runner/work/hpe-gauge-small/hpe-gauge-small/main/idf_component.yml`:

- `lvgl/lvgl` (v9)
- `joltwallet/littlefs`
- `espressif/esp_lcd_gc9a01`
- `espressif/esp_lcd_touch`
- `espressif/esp_lcd_touch_cst816s`

## Internal component documentation

- `/home/runner/work/hpe-gauge-small/hpe-gauge-small/main/README.md`
- `/home/runner/work/hpe-gauge-small/hpe-gauge-small/main/include/README.md`

## BSP pin configuration

Pins and app settings are exposed in Kconfig menu:

- `HPE Gauge BSP` section in menuconfig (`/home/runner/work/hpe-gauge-small/hpe-gauge-small/main/Kconfig.projbuild`)
- Default pin mapping is provided and can be changed there.

## Build

```bash
idf.py set-target esp32s3
idf.py build
```

## CI build workflow

GitHub Actions workflow:

- `/home/runner/work/hpe-gauge-small/hpe-gauge-small/.github/workflows/build-idf.yml`
- Runs on Linux runner using ESP-IDF container `espressif/idf:release-v6.0`
- Workflow page: https://github.com/D0kRay/hpe-gauge-small/actions/workflows/build-idf.yml

Artifacts produced by CI:

- `esp-idf-build` firmware artifacts (bin/elf/map/flasher args)
- `wiki-site` MkDocs wiki website build output

Download artifacts from the latest workflow run on the Actions page above.

## Mobile web UI

The firmware starts a WiFi AP and HTTP/WebSocket server.

- Connect phone to SSID from `HPE_WIFI_AP_SSID` (default: `hpe-gauge`)
- Open `http://192.168.4.1/`
- The page shows live speed/RPM plus IMU/RTC status over WebSocket
- The page also supports uploading a signed firmware image to OTA slots

## Console CAN config commands

```text
cancfg show
cancfg set speed 100 0 16 le 0 0.01 0
cancfg save
```

Format in `/littlefs/can_messages.cfg`:

```text
name,can_id_hex,start_bit,bit_length,endian,signed,factor,offset
```

- `endian`: `le` (Intel) or `be` (Motorola)
- `signed`: `0` unsigned, `1` signed
