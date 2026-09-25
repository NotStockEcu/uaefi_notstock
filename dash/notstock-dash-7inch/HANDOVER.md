# NOT STOCK dash — handover

Paste this at the start of a new chat. It is written for an assistant with no
memory of the previous conversation.

---

## What this is

A standalone gauge cluster for a turbocharged Nissan Micra K11 ("Zagorka"),
running on a **Waveshare ESP32-S3-Touch-LCD-7** (800x480 RGB panel) and fed by
a **rusEFI** ECU over CAN. Bare ESP-IDF and LVGL 8.4, no SquareLine Studio, no
Raspberry Pi, no operating system. Boots in well under a second and does not
care about being cut off mid-frame by the ignition key.

The design started from a mockup image the owner supplied (kept at
`assets/mockup.jpg`). The current firmware is a working, flashed, running
build — display, touch and the settings menu are all confirmed on hardware.

---

## Hardware, confirmed

Two boards are supported, selected by `DASH_BOARD` at the top of
`main/board.h`. Their RGB data and sync pins are identical; CAN and EXIO5 are
not.

| | LCD-7 | LCD-5 (SKU 28117) |
| --- | --- | --- |
| CAN TX / RX | GPIO20 / GPIO19 | GPIO15 / GPIO16 |
| EXIO5 | CAN/USB selector, must be driven high | DI1, an isolated input, must be left alone |
| Flash | 8 MB (N8R8), custom `partitions.csv` | 16 MB (N16R8) |
| Console | UART0 via CH343 | USB Serial/JTAG |

| Thing | Detail |
| --- | --- |
| Board | Waveshare ESP32-S3-Touch-LCD-7, 8 MB flash, 8 MB OPI PSRAM |
| Panel | 800x480 RGB565, EK9716/ST7262, PCLK 21 MHz |
| Touch | GT911 on I2C 0x5D (fallback 0x14), SDA 8, SCL 9, INT GPIO4 |
| IO expander | CH422G at 0x24: EXIO1 TP_RST, EXIO2 DISP, EXIO3 LCD_RST, EXIO4 SD_CS, EXIO5 USB(low)/CAN(high) |
| CAN | onboard transceiver on GPIO19/20, 120R jumper fitted |

Two hardware facts that constrain the design:

1. **GPIO19/20 are shared between USB-OTG and CAN**, selected by EXIO5. The
   firmware sets CAN, so the OTG port is dead at runtime. **Flash over the UART
   Type-C port.**
2. **EXIO2 is a plain display-enable line with no PWM.** There is no way to dim
   the backlight from firmware. Brightness is faked with a black overlay, which
   is why the setting floors at 15 %.

---

## Build and flash

```
cd C:\dash\notstock-dash-esp32
idf.py fullclean
idf.py build
idf.py -p COM4 flash monitor
```

ESP-IDF 5.5, target esp32s3. This folder is the 7 inch build
(`DASH_BOARD BOARD_WS_LCD7`, 8 MB flash, console on UART0, `partitions.csv`
with a 7.9 MB app). After switching from the 5 inch build, delete `sdkconfig`
and run `idf.py fullclean`, otherwise the old values win. LVGL 8.4 comes from the component manager and is
configured entirely through `sdkconfig.defaults` — there is no `lv_conf.h`.

Healthy boot log:

```
RGB panel up, 800x480
GT911 at 0x5D, id 911
TWAI up, base 0x200
```

---

## File map

```
main/
  main.c          board bring-up: I2C, CH422G, RGB panel, LVGL, touch, CAN
  board.h         pin map and CH422G bit masks
  rusefi_can.c/h  TWAI driver and the rusEFI verbose-CAN decoder
  settings.c/h    NVS-backed runtime config, the single source of all limits
  touch.c/h       GT911 driver plus LVGL pointer indev
  ui.c/h          the dash screen, alarm overlay, hidden menu trigger
  ui_menu.c/h     the settings screen; DASH_VERSION lives in the header
  icons.c         generated: 7 card/flag icons, ALPHA_8BIT
  dials.c         generated: dial faces, needles, hub, ~1.1 MB
  logo.c          generated: NOT STOCK wordmark, TRUE_COLOR_ALPHA
  fonts/          generated: 6 LVGL fonts from DejaVu Sans Condensed Bold
tools/
  gen_dials.py    renders the static gauge artwork -> main/dials.c
  gen_assets.py   traces assets/ -> main/icons.c and main/logo.c
  preview.py      renders the dash to PNG from the same constants as ui.c
  preview_menu.py renders the settings screen the same way
assets/
  icons_sheet.png  owner-supplied card icons, yellow on black
  mockup.jpg       original design mockup, source of the wordmark
build_art/         PNGs the generators emit, consumed by preview.py
rusefi_can_bridge.py  unrelated: the old Raspberry Pi route, kept for reference
```

Regenerating artwork:

```
python tools/gen_dials.py     # after changing dial colours, zones, radii
python tools/gen_assets.py    # after changing assets/
python tools/preview.py       # check the layout without flashing
python tools/preview_menu.py 0
```

---

## Architecture decisions worth not undoing

**The static gauge artwork is pre-rendered, not drawn by LVGL.** `gen_dials.py`
draws each dial face at 4x supersampling in Pillow — radial gradient, bevelled
edge, hairline minor ticks, antialiased scale labels — and emits RGB565. LVGL 8
can do none of that; earlier attempts to build the dials from `lv_meter`
primitives looked cheap and the owner rejected them twice. At runtime LVGL only
rotates a needle image, moves one arc and rewrites a number. Costs ~180 kB of
flash and the redraw is *cheaper* than the primitive version.

`LY_BAND_W` / `LY_BAND_MOD` in `ui.c` must stay in step with `R_BAND_OUT` /
`BAND_W` in `gen_dials.py`; they position the live boost fill arc over the
baked band. Same for `SWEEP_START` / `SWEEP` versus the
`lv_meter_set_scale_range` call.

**Icons are not squared.** The oil can compositions are twice as wide as tall;
a square box halves the drawing. Each is fitted to a 44x30 slot at its own
aspect, and `ui.c` places every icon by the centre read from its own image
header, so mixed sizes need no layout edits.

**The wordmark is a traced bitmap, not text.** Italic, tightly kerned, two
colours in one word. No single LVGL font does that.

**The RPM strip is on an integer grid.** 33 segments at a pitch of 8. An
earlier version used 40 segments across a 268 px column, which put the pitch at
6.7 and made every second segment land a pixel off. The owner spotted it as
"wavy". Do not reintroduce fractional pitch.

**The flash alarm is a shift light and nothing else.** Only RPM triggers the
full-screen red pulse. Every other limit turns its own tile, needle or readout
red and stops there. This was an explicit instruction after an earlier version
flashed on everything.

**Every limit treats 0 as off.** The two low-pressure limits additionally arm
themselves: a tile only warns after its channel has been seen *above* the limit
once since boot, and only above 400 rpm. Without this, an unwired pressure
sensor broadcasts a flat zero and the tile sits permanently red. This was a
real bug found on the car.

---

## rusEFI side

TunerStudio, CAN bus settings:

- Enable rusEFI verbose CAN: **on**
- rusEFI CAN data base address: **512** (0x200)
- ID width: **11 bit**, bitrate **500 kbit**
- Can Dash Type: **None**

Frames decoded (layout from
[can_verbose.cpp](https://github.com/rusefi/rusefi/blob/master/firmware/controllers/can/can_verbose.cpp)):

| ID | used for |
| --- | --- |
| base+0 | fan, check engine, rev limit flags |
| base+1 | RPM, timing, injector duty, VSS |
| base+2 | TPS |
| base+3 | MAP, coolant, intake air temp |
| base+4 | oil pressure, oil temp, battery voltage |
| base+7 | lambda, low-side fuel pressure |

Scaling constants: `PACK_MULT_PRESSURE` 30 (kPa), `PACK_MULT_LAMBDA` 10000,
`PACK_ADD_TEMPERATURE` 40, `PACK_MULT_VOLTAGE` 1000, `PACK_MULT_ANGLE` 50.

TWAI runs in **normal mode, not listen-only**, on purpose: on a two-node bus
the dash must ACK or rusEFI accumulates TX errors and eventually goes bus-off.
It never queues a transmission (`tx_queue_len = 0`).

**ALS is not in the verbose broadcast.** Send it from a Lua script on a spare
ID with the flag in byte 0 bit 0 and set `CAN_ALS_ID` in `rusefi_can.h`. Left
at 0 the ALS tile just reads OFF.

VSS is a uint8 in km/h, so speed tops out at 255. Irrelevant for this car.

---

## Settings menu

Long-press the bottom-right corner (96x46 invisible hit area, three dim dots
are the only hint). Values apply live, SAVE & CLOSE writes to NVS, DEFAULTS
restores. Build stamp bottom right.

Rows: Shift flash on/off, shift flash rpm, shift flash level, water/oil/intake
temp limits, oil and fuel pressure minimums, boost limit, AFR lean limit,
brightness, rev counter max, redline, fuel (Petrol/E85), baro offset, demo
mode.

Demo mode is a setting, not a compile-time flag — no reflash to bench test.

---

## Verification approach used throughout

The assistant could not see the panel, so everything was checked numerically.
Reusable checks, worth repeating after any layout change:

- **Collision sweep.** Instrument `ImageDraw.text` in `preview.py`, collect
  every text bounding box, report overlaps and out-of-bounds. Run it at nominal,
  all-zero and widest-string data ("-1.00", "8000", "255", three-digit tiles).
  This is how the `-1.00 bar` collision with the end-of-scale labels was found.
- **Container fit.** Measure each label with the real generated font metrics
  and compare against its LVGL parent box, since LVGL clips children.
- **Font glyph audit.** Parse the cmap tables out of `main/fonts/*.c` and check
  every string literal against the font it is drawn with. This is how the
  missing degree sign in `dash_lbl_18` was caught.
- **Constant cross-check.** Run the `LY_*` defines through `gcc -E` and compare
  against `preview.py`'s constants, so the preview cannot drift from the
  firmware.
- **Geometry probes.** Walk outward from the dial centre along the needle angle
  and report where its ink ends, to prove it clears the ticks and the band.
  Careful: naive colour probes catch the coloured band and the fill arc, not
  the needle. Probe at a value where the band under the needle is neutral.

A recurring failure mode during development: caching. `importlib` with a
constant temp filename served stale bytecode and produced three identical
"results" for three different inputs. Use unique filenames per run.

---

## Known gaps / untested

- **Touch is confirmed working**, display confirmed working, menu confirmed
  reachable. CAN decoding has **not** been verified against a live ECU yet.
- Brightness is a software overlay, not real dimming (hardware limitation).
- Glow around the RPM digits, present in the mockup, is not implemented. It
  would need pre-rendered glowing digit bitmaps, roughly 300 kB.
- Flash usage is ~1.5 MB of C arrays. Fits `SINGLE_APP_LARGE` on 8 MB. If a
  build ever fails on app size, switch `CONFIG_ESPTOOLPY_FLASHSIZE_8MB` to
  `_16MB` if the board is the 16 MB variant.
- The oil-can icons do not include the falling drop from the standard symbol.
  At 28 px it lands on the spout tip and merges into a blob. Heat waves under
  the can distinguish oil temp from oil press instead.

---

## Owner preferences

Czech, technical, direct. Wants terse answers, no flattery, no hedging, no
apologies, no em dashes. Says so explicitly when something looks bad, and is
right about it — treat "it looks like shit" as actionable and go find the
concrete defect rather than defending the work. When a defect cannot be
reproduced, say so plainly and ask for a photo rather than guessing.
