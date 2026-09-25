# NOT STOCK dash - ESP32-S3-Touch-LCD-7 + rusEFI (uaEFI)

Standalone gauge cluster for the **Waveshare ESP32-S3-Touch-LCD-7** (800x480).
No Raspberry Pi, no operating system, no SquareLine Studio. Reads rusEFI's
verbose CAN broadcast over the board's onboard TJA1051 transceiver and draws
the dash with hand-written LVGL. Boots in well under a second and does not care
if you cut power mid-frame.

Fonts and icons that SquareLine would normally generate are already in the
repo (`main/fonts/`, `main/icons.c`), built from DejaVu Sans Condensed Bold by
`lv_font_conv`, and from `tools/gen_icons.py`.

## Artwork pipeline

Two source files under `assets/` produce every non-gauge bitmap:

| file | produces |
| --- | --- |
| `icons_sheet.png` | the five card icons, traced out of the sheet |
| `mockup.jpg` | the NOT STOCK wordmark |

```bash
python tools/gen_assets.py     # rewrites main/icons.c and main/logo.c
idf.py build
```

The icons are traced by colour rather than brightness: the alpha comes from
`R - B`, so the yellow strokes survive and the white card labels cancel out.
The FAN and ALS glyphs are not in the sheet, so they are drawn in the same
script at 10x and downsampled.

Icons are deliberately **not** squared off. The oil can compositions are twice
as wide as they are tall and a square box would halve the drawing, so each is
fitted into a 44x30 slot at its own aspect ratio. `ui.c` positions every icon
by the centre read from its own header, so mixed sizes need no layout edits;
change `ICON_BOX` in the script and nothing else moves.

The wordmark is a traced bitmap rather than two text labels, because it is
italic, tightly kerned and two-coloured, and no single LVGL font does that.
Each pixel is snapped to either white or the brand yellow before scaling, so
the JPEG compression noise in the source does not reach the panel. `LOGO_W`
sets its rendered width.

To swap any of it, replace the file in `assets/` and re-run the script. If your
sheet has a different layout, adjust `CARDS`, `ICON_ROWS` and `LOGO_BOX` at the
top, they are plain pixel coordinates.

## The gauge artwork is pre-rendered

The dial face, its coloured band, the ticks and the scale labels never change,
so they are not rebuilt from LVGL primitives every frame. `tools/gen_dials.py`
draws them once in Pillow at 4x supersampling and writes `main/dials.c` as
RGB565 image data. That buys radial gradients, hairline ticks, a bevelled edge
and properly antialiased labels, none of which LVGL 8 can draw itself. The
needle and hub cap are images too. At runtime LVGL only rotates the needle,
moves one arc and rewrites a number.

Cost is about 180 kB of flash and the redraw gets *cheaper*, since a blit beats
dozens of arc and line draws.

```bash
python tools/gen_dials.py     # rewrites main/dials.c and build_art/*.png
idf.py build
```

Edit the colours, radii, tick counts and zone breakpoints at the top of that
script. Two values have to stay in step with `main/ui.c`: `R_BAND_OUT` and
`BAND_W` correspond to `LY_BAND_MOD` and `LY_BAND_W`, which position the live
boost fill arc on top of the baked band. `SWEEP_START` and `SWEEP` must match
the `lv_meter_set_scale_range` call.

## Checking the layout without flashing

```bash
python tools/preview.py
```

Renders an 800x480 PNG from the same geometry constants the firmware uses, and
composites the exact dial and needle bitmaps from `build_art/`, so the preview
is what the panel shows rather than an approximation of it.

## Layout preview

`notstock-dash-800x480-preview.png` is a pixel-accurate render of the layout,
produced by `tools/preview.py` from the same geometry constants the firmware
uses. Check it before you flash.

## Which board

This copy is built for the **Waveshare ESP32-S3-Touch-LCD-7** (800x480, N8R8).
It is a port of the working 5 inch build. The layout, CAN decoding and menu are
identical; only the board support changed. Both boards are handled by
`DASH_BOARD` in `main/board.h`, here set to `BOARD_WS_LCD7`.

| | this project (7 inch) | 5 inch |
| --- | --- | --- |
| `DASH_BOARD` | `BOARD_WS_LCD7` | `BOARD_WS_LCD5` |
| CAN TX / RX | GPIO20 / GPIO19 | GPIO15 / GPIO16 |
| CAN pins shared with USB OTG | yes, EXIO5 selects | no, dedicated |
| EXIO5 | CAN/USB selector, driven high | DI1, an isolated input |
| Module | N8R8, 8 MB flash | N16R8, 16 MB flash |
| Console | UART0 via CH343 ("UART" Type-C) | USB Serial/JTAG |

The RGB data and sync pins are identical on both, which is why a picture
appears either way. Flashing the wrong build gives a working picture but dead
CAN.

## Build

Needs ESP-IDF 5.1 or newer.

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

LVGL 8.4 is pulled automatically by the component manager and configured
entirely through `sdkconfig.defaults`, so there is no `lv_conf.h` to babysit.

If you built the 5 inch project in the same folder before, run
`idf.py fullclean` and delete `sdkconfig` first, otherwise the old 16 MB /
USB-console values survive in it and `sdkconfig.defaults` is ignored.

### Two Type-C ports, use the UART one

The 7 inch has two Type-C ports:

- **UART**: CH343 USB-UART bridge on GPIO43/44. Flash and `idf.py monitor`
  go through this one. It shows up as a CH343 COM port (Windows may need the
  WCH driver).
- **USB**: the ESP32-S3 native USB on GPIO19/20. **Dead while the dash runs.**
  GPIO19/20 are also the CAN pins, and the firmware drives EXIO5 on the
  CH422G high at boot, which switches them from USB to the CAN transceiver.

`sdkconfig.defaults` therefore keeps the log console on UART0.

### Flash size and partitions

`sdkconfig.defaults` is set to 8 MB to match the N8R8 module. The stock
`SINGLE_APP_LARGE` table caps the app at 1.5 MB, which the baked dial artwork
gets close to, so `partitions.csv` gives the app almost all of the 8 MB
(nvs, phy_init, one 7.9 MB factory app).

## Settings menu

The build stamp sits bottom right of that screen: `NOT STOCK v1.0` over the
compile date, the LVGL version and the IDF version. `DASH_VERSION` in
`main/ui_menu.h` is the bit to bump; the rest fills itself in from `__DATE__`,
`__TIME__` and `esp_get_idf_version()` at compile time, so it always tells the
truth about what is actually on the panel.

**Long-press the bottom-right corner of the screen** for about half a second.
The hit area is 96x46 px and invisible apart from three dim dots; nothing about
normal driving opens it. Values apply live as you adjust them, SAVE & CLOSE
writes them to NVS so they survive a power cut, DEFAULTS puts everything back.

| Setting | Range | Notes |
| --- | --- | --- |
| Shift flash | on/off | master switch for the full-screen red flash |
| Shift flash at | 0-9000 rpm | 0 disables it |
| Shift flash level | 10-100 % | peak opacity of the red wash |
| Water / oil / intake temp | | high limits |
| Oil press min, fuel press min | | low limits, see the note below |
| Boost limit | 0-2.5 bar | also turns the boost needle and readout red |
| AFR lean limit | 0-20.0 | 0 disables |
| Brightness | 15-100 % | see the note below |
| Rev counter max, redline | | rescales the strip and its numbers |
| Fuel | Petrol / E85 | sets stoichiometric AFR, 14.7 or 9.8 |
| Baro offset | 0.80-1.10 bar | what gets subtracted from MAP for gauge boost |
| Demo mode | on/off | synthetic data, no reflash needed |

**Brightness is software, not backlight.** EXIO2 on the CH422G is a plain
display-enable line with no PWM, so there is no way to dim the LEDs from
firmware. The setting lays a black wash over the picture instead, which lowers
apparent brightness but not power draw or black level. That is why it stops at
15 %.

### Why a tile limit can look like it does nothing

Every limit treats **0 as off**, including the low-pressure ones. Setting oil
press min to 0 disables it rather than turning the tile red at zero pressure.

The two low-pressure tiles also arm themselves. A tile only starts warning
after its channel has been seen *above* the limit at least once since
power-up, and only above 400 rpm. This matters because an oil or fuel pressure
sensor that is not wired, or not configured in rusEFI, broadcasts a flat zero,
which is below any limit, so the tile would sit permanently red. Once a channel
has read healthy the arming latches and a genuine pressure drop still shows
immediately. Changing a limit in the menu re-arms both channels.

## Shift flash

**Revs are the only thing that flashes the screen.** Every other limit turns
its own tile, needle or readout red and leaves it at that. Temperatures and
pressures creep, and strobing the panel while the driver is trying to read the
number that caused it is worse than useless. Revs are the one case where the
reaction has to happen inside a second, with your eyes on the road.

Above the set rpm the whole screen pulses red at about It is a square wave rather than a fade, because a hard flash is far
more noticeable in daylight and costs one opacity write per half period rather
than one per frame. The wash sits on LVGL's top layer, so it covers the dash
but does not block the menu, and the brightness dim sits above it on the system
layer so a dimmed screen also has a dimmer flash.

## Bench test without the car

Turn on Demo mode in the settings menu. A synthetic generator sweeps every
gauge and the link indicator reads DEMO. No rebuild, no reflash.

## Touch

GT911 on the shared I2C bus, address 0x5D, INT on GPIO4, reset on CH422G EXIO1.

**Coordinates are scaled, not clipped.** This board ships in 800x480 and
1024x600 flavours and the touch controller carries its own configuration, so it
can report on a different grid than the panel actually has. The driver reads
the controller's configured X and Y maxima at boot and scales every point onto
the panel. An earlier version discarded anything at or beyond 800x480, which on
a 1024x600 controller silently threw away the entire right and bottom of the
screen, including the corner the settings menu lives in.

The boot log states both grids:

```
GT911 at 0x5D, id 911, controller grid 1024x600, panel 800x480  -> scaling
```

and every press logs one line, `touch 743,451`. If presses appear in the log
but land in the wrong place, the scaling numbers in that boot line are the
thing to look at. If nothing appears at all, check the i2c scan line for 0x5D.
The INT line doubles as the address select during reset, so the driver holds it
low, releases reset, then hands the pin back as an input. If the controller
does not answer on 0x5D it retries 0x14. A failed probe is not fatal: the dash
runs as before, only the menu becomes unreachable.

## Wiring

| Board | To |
| --- | --- |
| CAN terminal H / L | rusEFI CAN H / L, twisted pair |
| CAN 120R | the 7 inch has a termination jumper. Leave it fitted if the dash is the far end of the bus, remove it otherwise |
| 5V | 12V to 5V buck, 1A minimum, switched with ignition |

Draw is roughly 450 mA at 5V with the backlight up.

## rusEFI side (TunerStudio, CAN bus settings)

- Enable rusEFI verbose CAN: **on**
- rusEFI CAN data base address: **512** (0x200)
- ID width: **11 bit**
- Bitrate: **500 kbit** (match `CAN_BITRATE_500` in `main/rusefi_can.h`)
- Can Dash Type: **None**

The dash runs TWAI in normal mode, not listen-only, on purpose. On a two node
bus the dash has to acknowledge frames or rusEFI piles up transmit errors and
eventually drops to bus-off. It never queues a transmission of its own
(`tx_queue_len = 0`).

Decoded frames, from
[can_verbose.cpp](https://github.com/rusefi/rusefi/blob/master/firmware/controllers/can/can_verbose.cpp):

| ID | Contents used |
| --- | --- |
| base+0 | Status: fan / fan2, check engine, rev limit |
| base+1 | RPM, ignition timing, injector duty, VSS |
| base+2 | TPS |
| base+3 | MAP, coolant, intake air temp |
| base+4 | oil pressure, oil temp, battery voltage |
| base+7 | lambda, low-side fuel pressure |

Scaling constants (`PACK_MULT_PRESSURE` 30, `PACK_MULT_LAMBDA` 10000,
`PACK_ADD_TEMPERATURE` 40, `PACK_MULT_VOLTAGE` 1000, `PACK_MULT_ANGLE` 50) come
from [rusefi_generated.h](https://rusefi.com/docs/html/rusefi__generated__cypress_8h_source.html).

**ALS is not in the verbose broadcast.** Send it yourself from a Lua script on
a spare ID with the flag in byte 0 bit 0, then set `CAN_ALS_ID` in
`main/rusefi_can.h`. Left at 0 the ALS tile just stays OFF. If you would rather
show check engine there, swap `d.als` for `d.cel` in `ui_timer_cb`.

## What to tune where

`main/rusefi_can.h`

- `CAN_BASE_ID`, `CAN_BITRATE_500`
- `AFR_STOICH` - 14.7 petrol, 9.76 E85
- `BARO_BAR` - what gets subtracted from MAP to give gauge boost

`main/ui.c`

- `LY_*` block at the top: every block position and size
- `RPM_MAX`, `RPM_REDLINE`, `RPM_SEGS`
- `card_cfg[]`: range, decimals, `warn_hi` / `warn_lo` per tile. A tile past
  its threshold turns its border, bar, icon and number red.
- `boost_cfg` / `afr_cfg` in `ui_create`: range, tick spacing, coloured zones
- `NEEDLE_SMOOTH`: 1.0 is instant, lower is lazier

Colours are the `C_*` defines at the top of `ui.c`.

Note on the boost scale: the mockup has zero at twelve o'clock *and* an even
-1 to 2 bar scale, which cannot both be true. The firmware uses a symmetric
250 degree sweep with even ticks, so zero sits upper-left. For zero exactly at
the top, change the `lv_meter_set_scale_range` call to a 270 degree range with
rotation 180.

The tick numbers sit inside the coloured band, matching the original mockup.
`LY_LABEL_GAP` controls how far inboard they sit: smaller pushes them out
toward the ticks, larger pulls them in toward the hub. Do not lower it much,
at 11 or less the label ink starts disappearing under the arc and minus signs
get eaten. `tools/preview.py` plus the radius figures in this README are how
that gets checked.

Layout budget on this panel, top to bottom:

| block | y | height |
| --- | --- | --- |
| gauges and rev counter | 4 | 286 |
| sensor cards | 302 | 104 |
| flags, wordmark, link | 418 | 46 |

The big gauge readouts sit *below* their dials rather than inside them. Inside
looks tighter but a 202 px dial cannot hold a coloured band, two tick sizes,
scale labels and a 48 px number without something colliding at the extremes of
the range (-1.00 bar was the case that broke it).

## Expected frame rate

Waveshare's own measurement on this board with ESP-IDF 5.3 is
[26 fps average for the LVGL benchmark at PCLK 21 MHz](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-7).
That number is for full-screen churn. This dash redraws only dirty rectangles
and caches every label so nothing is invalidated unless its text actually
changed, so the needles run smooth. The UI refresh timer is 40 ms.

`sdkconfig.defaults` already carries the performance flags Waveshare
recommends: 240 MHz, QIO flash, octal PSRAM, instructions and rodata fetched
from PSRAM, 64 byte cache lines, `-O2`, LVGL hot paths in IRAM.

## If something is wrong

I could not compile or run this against the real hardware, so budget an
evening. In rough order of likelihood:

1. **Screen stays black.** The CH422G IO expander is the suspect. This
   firmware writes mode byte 0x01 to I2C address 0x24 and then the output
   bitmap to 0x38, with backlight on EXIO2, LCD reset on EXIO3. Compare
   against `ESP_IOExpander_CH422G` in Waveshare's own demo if it misbehaves.
2. **Image drifts sideways or tears.** Raise `bounce_buffer_size_px` in
   `panel_init`, or drop `LCD_PCLK_HZ` to 16 MHz.
3. **Panel geometry off.** The porch values in `panel_init` are Waveshare's;
   ST7262 panels tolerate a wide range but a shifted image means these.
4. **Nothing on CAN.** Watch the serial log for TWAI bus-off warnings, and
   check the 120R termination jumper. The boot log states which pins TWAI came
   up on: it must say `tx 20 rx 19`. If it says `tx 15 rx 16` you are running
   the 5 inch build.

## Sources

- [Waveshare ESP32-S3-Touch-LCD-7 wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-7) - pin map, CH422G, performance notes
- [ESP-IDF RGB LCD driver](https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32s3/api-reference/peripherals/lcd/rgb_lcd.html)
- [LVGL 8.4 docs](https://docs.lvgl.io/8.4/)
- [rusEFI can_verbose.cpp](https://github.com/rusefi/rusefi/blob/master/firmware/controllers/can/can_verbose.cpp)
