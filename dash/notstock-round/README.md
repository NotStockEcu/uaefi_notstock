# NOT STOCK round gauge

One value at a time on a round display, swipe left / right for the next: water, oil, boost, intake, exhaust, engine
rpm, DPF load. Data comes over CAN the same way as on the 7" dash
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
- **DPF** is a placeholder page (0..100 %, warn 80) until its VW measuring
  value is sniffed; the scale follows what that turns out to be.

Pages, ranges, ticks and limits are the `PAGES` table in `gen_faces.py`
(written into `faces.h` for the UI); readout decimals and which pages keep a
peak are `FMT[]` in `ui_round.c`. Icons are vector drawings in the same
script (unit box 0..100), so they scale to any size.

Only one device may poll the OBD port: with the 7" dash on OBD-II as well,
one of the two has to listen only.

## Preview on the PC

    LVGL_DIR=/path/to/lvgl-8.4 python tools/preview.py
    LVGL_DIR=/path/to/lvgl-8.4 python tools/preview.py page=2 boost=1.8

Writes `preview/*.png`. After changing pages or artwork:
`python tools/gen_faces.py` (add `--size 466` for the AMOLED). Fonts:
`sh tools/gen_fonts.sh` (Orbitron, SIL OFL, `assets/fonts`).
