# NOT STOCK round gauge

One value at a time on a round display, swipe left / right for the next: water, oil, boost, intake, exhaust, engine
rpm. Long press anywhere for the menu: GAUGES, DPF STATUS, SETTINGS (to come). Data comes over CAN the same way as on the 7" dash
(`../notstock-dash-7inch`: rusEFI broadcast, or OBD-II plus the VW UDS
measuring values on a T5.1), through an SN65HVD230 board on two free GPIOs.

Target board: **Waveshare ESP32-S3-Touch-LCD-2.1** (480 x 480, ST7701 on
RGB like the 7" dash, CST820 touch). The UI is laid out from the centre and
also builds for a 1.32" AMOLED at 466 x 466 (`-DRND_SIZE=466`, sim
`make SIZE=466`).

**State: UI mock-up only.** `main/ui_round.c` is real LVGL 8.4 code and runs
in the PC simulator; panel, touch and CAN drivers come once the board is on
the bench.

![sheet](preview/sheet.png)

## The screen

![icons](preview/icons.png)

- **Pre-rendered faces** (`tools/gen_faces.py` -> `main/faces.c`): radial
  background, bezel line, the groove the value arc runs in, the red zone
  above the warn limit, ticks, scale numbers, the page's icon and title. One
  RGB565 image per page, 450 kB each in flash; nothing of it costs a frame.
- **Drawn live by LVGL** on top: the value arc with a two-layer glow, the
  readout (Orbitron Black 112, 84 for four digits), unit, peak since power-on
  (not for rpm) and the page dots. Over the limit arc, glow and number go
  red.
- EXHAUST and ENGINE scales are in hundreds / thousands (`x100`, `x1000`).
- Swipe changes the page; the arc sweeps up from the bottom of the new scale
  and the readout fades in.
- No CAN: `--` and NO DATA; a value the car does not give: NOT READ.
- Long press: the menu.

Pages, ranges, ticks and limits are the `PAGES` table in `gen_faces.py`
(written into `faces.h` for the UI); readout decimals and which pages keep a
peak are `FMT[]` in `ui_round.c`. Icons are vector drawings in the same
script (unit box 0..100), so they scale to any size.

Only one device may poll the OBD port: with the 7" dash on OBD-II as well,
one of the two has to listen only.

## Menu and DPF status

![menu](preview/menu.png) ![dpf](preview/dpf.png)

**DPF STATUS** (`ui_round_dpf.c`), from the VW measuring values the 7" dash
already reads (UDS 114F / 114E / 14F5 / 1156 / 1044):

- Outer arc and a filter drawing that fills from the inlet side: calculated
  soot mass, full at 40 g. White, amber from 70 % of the warn level, red over
  it. The warn level (24 g) is a guess until a regeneration is seen
  (`SOOT_WARN`).
- Readout in g, measured soot under it, then differential pressure, filter
  temperature and km since the last regeneration.
- The pill at the bottom: how full against the warn level, or REGENERATING.
- Swipe or long press to leave.

**Regeneration**: once the simulated filter temperature passes 400 degC
(`REGEN_TEMP`, ending 50 degC below), a popup comes up over whatever screen
is shown, with three beeps; when it ends, a green one with the soot before
and after and one beep. Tap closes it, otherwise it goes after 10 s. While
it lasts the gauges show DPF REGEN under the page dots, the DPF screen
glows orange, and the menu's DPF icon is orange.

![regen-start](preview/regen-start.png) ![dpf-regen](preview/dpf-regen.png)

The beep is `rnd_beep()`, provided by the platform: on the Waveshare 2.1"
the buzzer sits on the TCA9554 expander (EXIO8).

## Preview on the PC

    LVGL_DIR=/path/to/lvgl-8.4 python tools/preview.py
    LVGL_DIR=/path/to/lvgl-8.4 python tools/preview.py page=2 boost=1.8
    LVGL_DIR=/path/to/lvgl-8.4 python tools/preview.py screen=dpf regen=start

Writes `preview/*.png`. After changing pages or artwork:
`python tools/gen_faces.py` (add `--size 466` for the AMOLED). Fonts:
`sh tools/gen_fonts.sh` (Orbitron, SIL OFL, `assets/fonts`).
