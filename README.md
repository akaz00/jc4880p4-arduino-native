# Guition JC4880P443 on Arduino IDE (plain ESP-IDF calls, no PlatformIO)

Working Arduino IDE sketches for the **Guition JC4880P443C_I_W** (ESP32-P4 + ESP32-C6, 4.3" touch display):
display, LVGL 8.3, GT911 touch and Wi-Fi, each as a single `.ino` with no extra display/touch library.

> 한국어 요약: Guition JC4880P443 보드를 **Arduino IDE만으로** 쓰기 위한 예제 모음입니다.
> `esp_lcd_st7701` / `ESP32_Display_Panel` 경로로는 화면이 안 켜져서, `ultramcu/guition-jc4880p4-bsp`의 검증된 브링업 값을
> 순정 ESP-IDF API 호출로 옮겼습니다. 터치는 `Wire`로 GT911을 직접 읽습니다.

## Target board

| Item | Value |
|---|---|
| Board | Guition JC4880P443C_I_W (module JC-ESP32P4-M3) |
| MCU | ESP32-P4 (chip revision v1.3 as reported by `esp_chip_info()`), 16 MB flash, 32 MB PSRAM |
| Wi-Fi/BT | ESP32-C6 co-processor over SDIO (ESP-Hosted) |
| Display | 4.3" 480x800 (portrait) ST7701S, MIPI-DSI 2-lane |
| Touch | GT911 (I2C, SDA=GPIO7 / SCL=GPIO8 / RST=GPIO3, address 0x5D or 0x14) |

## The problem

On Arduino IDE the usual routes left this board with a black screen (and at times a hang or reboot loop):

- `ESP32_Display_Panel` with a custom board config, which uses the `esp_lcd_st7701` component.
- According to the BSP notes credited below, the stock `esp_lcd_st7701` component does not light this panel.

What worked: skip that component and do the bring-up by hand with plain ESP-IDF APIs that the Arduino-ESP32 core already ships:

`LDO (chan 3, 2500 mV)` -> `DSI bus (2 lanes, 500 Mbps)` -> `DBI IO` -> `generic DPI panel (34 MHz, 480x800, RGB565)` -> `GPIO5 reset pulse` -> `ST7701 init table via esp_lcd_panel_io_tx_param()` -> `esp_lcd_panel_init()` -> `backlight (GPIO23)`

The touch controller is read directly over `Wire`, because `esp_lcd_touch_gt911` is an ESP-IDF component and is not bundled with the Arduino core.

## Examples

| Folder | What it does |
|---|---|
| `examples/display_colorbar` | Panel bring-up only, draws 7 color bars. Start here. |
| `examples/lvgl_touch_hello` | LVGL 8.3 in portrait (480x800) + GT911 touch. A label and a button with a press counter. |
| `examples/gui_tabview_demo` | Fixed status bar, 3 tabs (Control / Status / Input), common widgets, on-screen keyboard. |
| `examples/wifi_connect` | Wi-Fi STA connect through the C6. Fill in your SSID/password first. |

## Requirements

- Arduino IDE 2.x
- **ESP32 board package by Espressif Systems, 3.x** with ESP32-P4 support. Tested version: _fill in exact version before publishing_
- **LVGL 8.3.11** (Library Manager). The LVGL examples were tested with this version only.
  - LVGL needs a `lv_conf.h` next to the `lvgl` library folder. Copy `extras/lv_conf.h` from this repo to `Documents/Arduino/libraries/lv_conf.h` (it sets `LV_COLOR_DEPTH 16`).
  - The examples only use LVGL's default font (Montserrat 14), which has no Korean/CJK glyphs.
  - `LV_MEM_SIZE` is 48 KB in that file. Raise it if you build bigger UIs and hit memory errors.
- No other libraries. Display, touch and Wi-Fi use the core's own headers and `Wire` / `WiFi`.

### Arduino IDE board settings (Tools menu)

| Setting | Value |
|---|---|
| Board | ESP32P4 Dev Module |
| USB CDC On Boot | Enabled |
| Chip Variant | Before v3.00 |
| Flash Size | 16MB (128Mb) |
| Flash Mode / Frequency | QIO / 80MHz |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |
| **PSRAM** | **Enabled** |
| Upload Mode | UART0 / Hardware CDC |
| USB Mode | USB-OTG (TinyUSB) |

Serial Monitor: 115200 baud.

## Gotchas we hit

- **`no memory for frame buffer`**: PSRAM is not enabled in the Tools menu. Two 480x800 RGB565 frame buffers need PSRAM.
- **No serial output at all** even though upload succeeds: check `USB CDC On Boot` and use the board's USB-OTG (native USB) port for the Serial Monitor.
- **`designator order ... does not match declaration order`**: in C++ the fields of `esp_lcd_dpi_panel_config_t` must be initialised in declaration order (`virtual_channel` first). The examples already do this.
- **LDO channel**: the DSI PHY supply is on-chip LDO channel **3** at 2500 mV (ESP-IDF channel numbering).
- **Wi-Fi**: call `WiFi.setPins(clk, cmd, d0, d1, d2, d3, rst)` before `WiFi.begin()`. If it connects and then keeps dropping, the C6 ESP-Hosted firmware version may not match the core; see the BSP README below for the C6 fix.

## Status

Tested on one board:

- Display bring-up and color bars: works
- LVGL 8.3.11 portrait rendering (two full-size frame buffers, full refresh): works
- GT911 touch via `Wire`: works
- Wi-Fi STA connect and DHCP: works

Not covered here: SD card, audio codec, camera, RS-485, landscape/rotation (PPA), long-run Wi-Fi stability.

## Credits

The display bring-up recipe (LDO channel and voltage, DSI lane rate, DPI clock and timings, reset timing, the ST7701S init command table) and the Wi-Fi SDIO pin numbers come from:

- **[ultramcu/guition-jc4880p4-bsp](https://github.com/ultramcu/guition-jc4880p4-bsp)** (MIT, (c) 2026 ultramcu): `board_p4.c`, `board_p4_pins.h`, `board_p4_st7701_init.h`. This repo re-implements that approach as Arduino IDE sketches. Please star and read theirs for PlatformIO usage, PPA landscape rotation, SD card and the C6 Wi-Fi firmware fix.
- Their field notes, pinout and schematic links: [ultramcu/guition-jc4880p443c-i-w](https://github.com/ultramcu/guition-jc4880p443c-i-w)

Also referenced while debugging (DPI timing cross-check):

- [csvke/esp32_p4_jc4880p433c_bsp](https://github.com/csvke/esp32_p4_jc4880p433c_bsp)

Thanks to all of them. This repo is not affiliated with Guition.

Developed with the help of [Claude](https://claude.ai) (Anthropic): debugging, adapting the bring-up recipe to Arduino IDE, and writing the docs. All hardware testing was done by hand on a real board.

## License

MIT, see [LICENSE](LICENSE). Portions are derived from the MIT-licensed projects listed under Credits.
