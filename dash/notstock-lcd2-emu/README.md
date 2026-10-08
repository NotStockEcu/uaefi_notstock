# NOT STOCK 2" gauge, ECUMaster EMU Black

Waveshare **RP2350-Touch-LCD-2** (RP2350, 2" IPS 240 x 320 ST7789T3 on
SPI, CST816D touch, QMI8658) reading an **ECUMaster EMU Black** over its
CAN stream, as [`../notstock-round-emu`](../notstock-round-emu) does on the
2.1" round board: the same decoder (`../notstock-round-emu/main/emu_stream.c`),
the same honeycomb, NOT STOCK yellow.

![sheet](preview/sheet.png)

Page 1, page 2 (fan running), alarm, vacuum, failed sensors, no data.

**State: the pages, in the simulator. The board layer (display, touch,
CAN) is not written yet: it waits for the board's internal pin table.**

## The pages

Swipe left / right. At the top: the link dot (green: stream frames
coming, red: none), NOT STOCK with the page dots, FAN lit blue while the
EMU runs the coolant fan (OUTFLAGS4 bit 1). Every channel is a card whose
left edge is yellow, orange (warn), red (alarm, the value blinks) or blue
(cold).

- **Page 1**: BOOST (MAP - BARO, an LED bar of 0.1 bar segments from -1
  to 2.5, the vacuum dim, the white cap is the MAX since power-on),
  LAMBDA (a needle on 0.70 .. 1.30, the zones are the alarm limits, AFR
  under the title), IAT, CLT.
- **Page 2**: throttle, RPM, oil temperature, oil pressure (no alarm
  under 400 rpm), battery, EGT1.

| Channel | Orange | Red |
| --- | --- | --- |
| BOOST | from 1.8 bar | from 2.2 |
| LAMBDA | under 0.762, over 1.034 | under 0.714, over 1.088 |
| IAT | over 50 | from 65 |
| CLT | over 100; blue under 60 | from 108 |
| RPM | from 6500 | from 7200 |
| OIL degC | over 120; blue under 60 | from 135 |
| OIL bar | under 1.0 | under 0.5 |
| BATT | under 12.0, over 14.8 | under 11.5, over 15.2 |
| EGT | from 900 | from 950 |

Limits and ranges: `CH[]` in `main/ui_l2.c`. A failed sensor (the EMU's
error flags) shows SENSOR ERR, a frame older than 1 s `--`. The
background: `tools/gen_bg.py` -> `main/l2_bg.c`.

## Simulator

```
make -C tools/sim LVGL_DIR=/path/to/lvgl-8.4
tools/sim/build/sim out.ppm boost=1.45 lambda=0.82 page=0
```

Inputs: see `tools/sim/sim.c`.
