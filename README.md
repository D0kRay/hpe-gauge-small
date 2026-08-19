# hpe-gauge-small

ESP32-S3 ESP-IDF project for a 240x240 round gauge with:

- GC9A01 SPI display
- CST816 touch controller
- LVGL 9 UI (speed + RPM gauge and a starter menu)
- TWAI/CAN interface (for MCP transceiver)
- USB Serial/JTAG console
- QMI8658 IMU and PCF85063 RTC presence checks
- LittleFS-backed CAN signal mapping (`/littlefs/can_messages.cfg`)

## Managed components

Configured in `/home/runner/work/hpe-gauge-small/hpe-gauge-small/main/idf_component.yml`:

- `lvgl/lvgl` (v9)
- `joltwallet/littlefs`
- `espressif/esp_lcd_gc9a01`
- `espressif/esp_lcd_touch`
- `espressif/esp_lcd_touch_cst816s`

## BSP pin configuration

Pins and app settings are exposed in Kconfig menu:

- `HPE Gauge BSP` section in menuconfig (`/home/runner/work/hpe-gauge-small/hpe-gauge-small/main/Kconfig.projbuild`)
- Set SPI, I2C, TWAI pins and bitrate there.

## Build

```bash
idf.py set-target esp32s3
idf.py build
```

## Console CAN config commands

```text
cancfg show
cancfg set speed 100 0 2 0.01 0
cancfg save
```

Format in `/littlefs/can_messages.cfg`:

```text
name,can_id_hex,start_byte,length_bytes,scale,offset
```
