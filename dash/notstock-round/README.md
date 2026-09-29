# NOT STOCK round gauge

One value at a time on a round display, swipe left / right for the next: water, oil, boost, intake, exhaust, engine
rpm. Data comes over CAN the same way as on the 7" dash
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

- Outer ring: dark track, dark red zone above the warn limit, white value
  arc. Over the limit the arc and the number go red.
- Ticks and scale numbers inside the ring; EXHAUST and ENGINE scales are in
  hundreds / thousands (`x100`, `x1000` under the dial).
- Middle: title, value (Orbitron Black 112, 84 for four digits), unit, and
  the highest value since power-on (not for rpm).
- Bottom: page dots, the active one is the long one.
- Swipe changes the page; the value arc sweeps up from the bottom of the new
  scale and the middle fades in.
- No CAN: `--` and NO DATA; a value the car does not give: NOT READ.

Scales and limits are in the `PAGE[]` table at the top of `ui_round.c`.

Only one device may poll the OBD port: with the 7" dash on OBD-II as well,
one of the two has to listen only.

## Preview on the PC

    LVGL_DIR=/path/to/lvgl-8.4 python tools/preview.py
    LVGL_DIR=/path/to/lvgl-8.4 python tools/preview.py page=2 boost=1.8

Writes `preview/*.png`. `sh tools/gen_fonts.sh` regenerates the fonts
(Orbitron, SIL OFL, `assets/fonts`).
