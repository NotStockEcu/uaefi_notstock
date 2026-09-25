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

The first design started from a mockup image the owner supplied (kept at
`assets/mockup.jpg`). Display, touch and the settings menu of that build are
confirmed on hardware.

v2.0 replaced the screen with a classic analogue cluster after a reference
picture from the owner: speedometer (km/h) and rev counter in the middle,
water and intake air temperature on the left in one style, turbo and AFR on
the right in another, every gauge with a needle and a digital readout, no
warning lamps. The board support is unchanged; the new gauges have been
checked in the host simulator (`tools/preview.py`) but not yet on the panel.

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
  dials.c/h       generated: scale faces, needles, hubs and their geometry
  icons.c         generated: card/flag icons, ALPHA_8BIT (water, iat used)
  logo.c          generated: NOT STOCK wordmark, TRUE_COLOR_ALPHA
  fonts/          generated: 6 LVGL fonts from DejaVu Sans Condensed Bold
tools/
  gen_dials.py    renders the gauge artwork -> main/dials.c, main/dials.h
  gen_assets.py   traces assets/ -> main/icons.c and main/logo.c
  preview.py      builds tools/sim and renders preview/*.png
  sim/            host build of the real ui.c + LVGL, stubs for ESP-IDF
assets/
  icons_sheet.png  owner-supplied card icons, yellow on black
  mockup.jpg       original design mockup, source of the wordmark
build_art/         PNGs gen_dials.py emits, for eyeballing
preview/           simulator renders
```

Regenerating artwork:

```
python tools/gen_dials.py     # after changing scales, zones, colours, needles
python tools/gen_assets.py    # after changing assets/
python tools/preview.py       # check the result without flashing
```

---

## Architecture decisions worth not undoing

**The static gauge artwork is pre-rendered, not drawn by LVGL.** `gen_dials.py`
draws each scale at 4x supersampling in Pillow and emits RGB565. Earlier
attempts to build dials from `lv_meter` primitives looked cheap and the owner
rejected them twice. At runtime LVGL only rotates needle images and rewrites
numbers.

**Geometry comes from `dials.h`, which `gen_dials.py` writes.** Sweep angles,
ranges, the redline and needle pivots are generated next to the images, so
`ui.c` never repeats a number that has to match the art.

**Faces are plain images, the lv_meter only draws the needle.** `lv_meter`
puts its centre at (w/2, w/2) from its top-left corner, not the middle of the
object, so a non-square face used as its background shifts the needle off the
pivot. Each face is an `lv_img` centred on the pivot; a transparent, square,
non-clickable meter centred on the same point draws the needle; the hub cap
image goes on top.

**The preview is the real code.** `tools/sim` compiles `ui.c`, `ui_menu.c`,
`settings.c`, fonts and art against LVGL on the host with stub ESP-IDF
headers. The old Python preview mirrored the layout constants and could drift;
it is gone. On the host LVGL's heap is 256 kB because pointers are 8 bytes;
the panel has 64 kB.

**Icons are not squared.** Each is fitted to a 44x30 slot at its own aspect,
and `ui.c` places every icon by the centre read from its own image header, so
mixed sizes need no layout edits.

**The wordmark is a traced bitmap, not text.** Italic, tightly kerned, two
colours in one word. No single LVGL font does that.

**Temperature min/max labels hang under the ends of the arc.** Placed along
the end radius, the needle lies across them when it rests on the stop.

**The flash alarm is a shift light and nothing else.** Only RPM triggers the
full-screen red pulse. Every other limit turns its own readout red and stops
there. This was an explicit instruction after an earlier version
flashed on everything.

**Every limit treats 0 as off.** The low-pressure limits and their arming
logic went with the oil and fuel pressure tiles in v2.0; the decoder still
reads those channels.

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

`CAN_ALS_ID` in `rusefi_can.h` is still decoded but nothing shows it since
v2.0 dropped the flag bar.

VSS is a uint8 in km/h, so speed tops out at 255. Irrelevant for this car.

---

## Settings menu

Long-press the bottom-right corner (96x46 invisible hit area, three dim dots
are the only hint). Values apply live, SAVE & CLOSE writes to NVS, DEFAULTS
restores. Build stamp bottom right.

Rows: Shift flash on/off, shift flash rpm, shift flash level, water/intake
temp limits, boost limit, AFR lean limit,
brightness, fuel (Petrol/E85), baro offset, demo mode. Rev counter max and
redline are baked into the artwork since v2.0 (`RPM_MAX`, `RPM_REDLINE` in
`gen_dials.py`). `settings.c` VER went to 2, so old NVS contents are dropped
once and the defaults load.

Demo mode is a setting, not a compile-time flag — no reflash to bench test.

---

## Verification approach used throughout

The assistant cannot see the panel. Since v2.0 it can see the real UI code
rendered on the host: `python tools/preview.py` and look at `preview/*.png`.
Worth doing after any change:

- **Extreme values.** Render at nominal, all-minimum and all-maximum inputs
  ("-1.00" boost, "238" km/h, "7400" rpm, "-20" degC, "100" degC). Wide strings
  and needles resting on the stops are where collisions show up.
- **States.** `link=0` for NO CAN, `demo=1 t=...` for the demo and shift flash,
  `screen=menu` for the settings screen.
- **Font glyph audit.** Parse the cmap tables out of `main/fonts/*.c` and check
  every string literal against the font it is drawn with. A missing glyph
  renders as a box in the preview too (the menu's middle dot was caught that
  way).
- **Firmware build.** The simulator does not compile `main.c`, `touch.c` or
  `rusefi_can.c`; run `idf.py build` as well.

A recurring failure mode during development: caching. `importlib` with a
constant temp filename served stale bytecode and produced three identical
"results" for three different inputs. Use unique filenames per run.

---

## Known gaps / untested

- **Touch is confirmed working**, display confirmed working, menu confirmed
  reachable on the v1 screen. The v2.0 gauges are verified in the simulator
  and compile for the target, but have not been on the panel yet: needle
  smoothness at 25 Hz with six rotating needles is the thing to watch.
- CAN decoding has **not** been verified against a live ECU yet.
- Brightness is a software overlay, not real dimming (hardware limitation).
- The app is about 1 MB; `partitions.csv` gives it 7.9 MB.
- `icons.c` still carries the oil, fuel, fan and flame icons nobody draws any
  more. Harmless, about 6 kB.

---

## Owner preferences

Czech, technical, direct. Wants terse answers, no flattery, no hedging, no
apologies, no em dashes. Says so explicitly when something looks bad, and is
right about it — treat "it looks like shit" as actionable and go find the
concrete defect rather than defending the work. When a defect cannot be
reproduced, say so plainly and ask for a photo rather than guessing.
