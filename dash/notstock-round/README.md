# NOT STOCK round gauge

One value at a time on a round display, swipe left / right for the next: water, oil, boost, intake, exhaust, engine
rpm. Long press anywhere for the menu: GAUGES, DPF STATUS, SETTINGS.
Double tap: night (backlight down), double tap again: day. Data comes over CAN the same way as on the 7" dash
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
  background, bezel line, the groove the value arc runs in, ticks, scale
  numbers, the page's icon and title. The red zone above the warn limit is
  drawn live over the groove, since the limit is a setting. One
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

## Boot

![boot-in](preview/boot-in.png) ![boot-logo](preview/boot-logo.png) ![boot-cross](preview/boot-cross.png)

The NOT STOCK. / NOT STABLE. badge fills the round panel: it comes out of
black (1.2 s, eased), holds (1.5 s) and cross-fades into the gauges
(0.9 s), whose arc sweeps up as they appear. `ui_round_create(true)`; the
timings are `RND_BOOT_*` in `ui_round.h`. The logo is `tools/gen_splash.py`
from `assets/splash.png` (the same file and keying as the 7" dash), 456 px,
406 kB of RGB565 in flash.

On the panel the cross-fade is a full-screen blend per frame; if LVGL is too
slow for it there, the 7" dash's way (blending straight in the framebuffer,
`boot_anim.c`) is the fallback.

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

## Looks

SETTINGS -> LOOK, applies at once. Menu, DPF status and settings stay the
same in every look.

![retro](preview/retro-boost.png) ![futuro](preview/futuro-boost.png)

- **NOTSTOCK**: the faces above, glowing value arc.
- **RETRO** (`ui_look_retro.c`): a mechanical instrument of the VDO kind.
  Chrome bezel, black dial, white print, a glass sheen, all pre-rendered
  per page (`draw_face_retro` in `gen_faces.py`). Live: the red band from
  the warn limit, a red needle with a shadow over a black hub, a thin
  orange tell-tale needle left at the peak (none for rpm), and the value in
  a small window (Barlow Condensed, SIL OFL).
- **FUTURO** (`ui_look_futuro.c`): one shared background (black, a hex grid
  fading to the rim, thin cyan rings) and 46 segments that light up to the
  value, cyan into magenta; dim red past the warn limit, all red over it.
  Icon and title on top, a pale neon readout, the scale's ends underneath.

A look is three functions (`rnd_look_t` in `ui_round_int.h`: build, page,
draw); `ui_round.c` hands it the page and a smoothed value every frame.
Flash: NOTSTOCK and RETRO faces 2.8 MB each, FUTURO's background 450 kB.

## Settings

![settings](preview/settings.png) ![limits](preview/limits.png)

Menu -> SETTINGS (`ui_round_set.c`); a long press goes one level back and
stores them (`rnd_settings_save()`, the platform's NVS).

- **LOOK**: NOTSTOCK, RETRO, FUTURO.
- **NIGHT 30 %**: the backlight at night, tap for the next step (10 to
  50 %). Night itself is a double tap on the gauges or the DPF screen; a
  toast says NIGHT 30 % or DAY for a second. Kept over power-off. The
  backlight is the platform's `rnd_backlight()`.
- **BEEP ON / OFF**: the regeneration beeps. The popup comes either way.
- **LIMITS**: one warn limit at a time, - and + (hold to repeat), swipe for
  the next: water, oil, boost, intake, exhaust, engine rpm, DPF soot. A
  gauge goes red over its limit and its red zone starts there; the DPF soot
  limit is what the DPF screen measures fullness against. Ranges, steps and
  defaults: `RND_LIMIT[]` in `ui_round_set.c`.

## Preview on the PC

    LVGL_DIR=/path/to/lvgl-8.4 python tools/preview.py
    LVGL_DIR=/path/to/lvgl-8.4 python tools/preview.py page=2 boost=1.8
    LVGL_DIR=/path/to/lvgl-8.4 python tools/preview.py screen=dpf regen=start
    LVGL_DIR=/path/to/lvgl-8.4 python tools/preview.py screen=limits limit=2

Writes `preview/*.png`. After changing pages or artwork:
`python tools/gen_faces.py` (add `--size 466` for the AMOLED). Fonts:
`sh tools/gen_fonts.sh` (Orbitron, SIL OFL, `assets/fonts`).
