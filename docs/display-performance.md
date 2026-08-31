# GC9A01 display and LVGL performance tuning

## Panel summary from the datasheet

The GC9A01 is a 240 x 240 color TFT panel with a 16-bit RGB565 data format. For a full-screen refresh:

- pixels per frame = 240 x 240 = 57,600
- bytes per pixel = 2 bytes (RGB565)
- bytes per full frame = 115,200 bytes

This is important because it tells us the graphics pipeline must move roughly 0.115 MB per full refresh. At 80 MHz SPI, the raw bus throughput is roughly 10 MB/s in ideal conditions, so even a full-screen repaint is not inherently the bottleneck on the ESP32-S3. The real bottleneck is the combination of:

1. LVGL render work
2. buffer size / flush cadence
3. SPI transfer size and queueing
4. software render overhead on the ESP32 CPU

## Board and driver tuning already applied

The current driver was using a smaller LVGL buffer and a conservative refresh cadence. The key changes for higher performance are:

- increase the LCD SPI clock to 80 MHz
- increase the LVGL buffer height to 80 scan lines
- keep LVGL running in a tight loop with a 1 ms tick and 2 ms task cadence
- compile the firmware with performance optimization instead of size optimization

These changes reduce the number of flushes and keep the refresh path closer to the panel’s usable bandwidth.

## Why the UI was limited to around 5 FPS

The project had several settings that were working against the display:

- `CONFIG_COMPILER_OPTIMIZATION_SIZE=y` was forcing size-optimized code
- the LVGL refresh period was relatively coarse
- the draw buffer was smaller than ideal for a 240 x 240 panel
- the LCD SPI clock was intentionally conservative

The result is that LVGL was spending too much time doing redraw work and flushes rather than keeping up with the panel’s real bandwidth.

## Recommended target behavior

With the updated configuration, the goal is to push the screen toward 20–30 FPS for typical UI state changes, while maintaining a smooth line/label refresh on the 240 x 240 display.

The realistic upper bound is not “unlimited FPS” because the display itself is a 240 x 240 TFT, but the ESP32-S3 can comfortably drive a panel of this size well above 5 FPS when the driver and LVGL settings are tuned correctly.

## Files tied to the tuning

- [main/display.c](main/display.c)
- [sdkconfig.defaults](sdkconfig.defaults)

## Practical next step

If you still see lower-than-expected FPS after this change, the next things to check are:

- whether the code is re-rendering the whole screen on every gauge update
- whether LVGL labels are being invalidated repeatedly without a real visual change
- whether the UI refresh timer is being called in too many unnecessary branches
