# DIN Fonts for LVGL

This project includes DIN-like fonts sourced from GitHub:

- Repository: https://github.com/CyberFei/D-DIN-PRO
- License: SIL Open Font License 1.1
- Local license copy: `main/fonts/OFL-1.1.txt`

## Included TTF files

Stored under `main/fonts/ttf/`:

- D-DIN-PRO-300-Light.ttf
- D-DIN-PRO-400-Regular.ttf
- D-DIN-PRO-500-Medium.ttf
- D-DIN-PRO-600-SemiBold.ttf
- D-DIN-PRO-700-Bold.ttf
- D-DIN-PRO-800-ExtraBold.ttf
- D-DIN-PRO-900-Heavy.ttf

## Generated LVGL C fonts

Stored under `main/fonts/generated/` and compiled into firmware:

- lv_font_ddin_regular_14.c
- lv_font_ddin_regular_16.c
- lv_font_ddin_medium_20.c
- lv_font_ddin_bold_20.c

## Regenerate fonts

Requires Node.js and `lv_font_conv`:

```bash
npx --yes lv_font_conv --font main/fonts/ttf/D-DIN-PRO-400-Regular.ttf --size 14 --bpp 4 --format lvgl --range 0x20-0x7F,0xB0 --output main/fonts/generated/lv_font_ddin_regular_14.c
npx --yes lv_font_conv --font main/fonts/ttf/D-DIN-PRO-400-Regular.ttf --size 16 --bpp 4 --format lvgl --range 0x20-0x7F,0xB0 --output main/fonts/generated/lv_font_ddin_regular_16.c
npx --yes lv_font_conv --font main/fonts/ttf/D-DIN-PRO-500-Medium.ttf --size 20 --bpp 4 --format lvgl --range 0x20-0x7F,0xB0 --output main/fonts/generated/lv_font_ddin_medium_20.c
npx --yes lv_font_conv --font main/fonts/ttf/D-DIN-PRO-700-Bold.ttf --size 20 --bpp 4 --format lvgl --range 0x20-0x7F,0xB0 --output main/fonts/generated/lv_font_ddin_bold_20.c
```
