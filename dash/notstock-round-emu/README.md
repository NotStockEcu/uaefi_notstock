# NOT STOCK round gauge, ECUMaster EMU Black

The 2.1" round gauge of [`../notstock-round`](../notstock-round) (Waveshare
**ESP32-S3-Touch-LCD-2.1**, same board, same wiring, same NOT STOCK boot
logo) reading an **ECUMaster EMU Black** over its CAN stream instead of
OBD-II, drawn after ECUMaster's own round gauges: a dark honeycomb, a
vertical bar with its scale on the left, channels stacked on the right with
their maximum since power-on under them.

![sheet](preview/sheet.png)

Page 1, page 2, the fan running, alarm (red, the value blinking), a failed
sensor, no data.

**State: built, decoder tested on a PC, the pages checked in the simulator;
not tried on the car yet.**

## The pages

Swipe left / right. At the top: the link dot (green: stream frames coming,
red: none) and FAN, lit blue while the EMU runs the coolant fan (OUTFLAGS4
bit 1).

| Page | Bar (left) | Right |
| --- | --- | --- |
| 1 | BOOST, bar: MAP - BARO, -1 .. 2.5 | AFR, Throttle % |
| 2 | CLT, degC: 20 .. 120 | IAT, degC |

| Channel | Orange | Red |
| --- | --- | --- |
| BOOST | from 1.8 (the orange line) | from 2.2 |
| AFR | under 11.2, over 15.2 | under 10.5, over 16.0 |
| CLT | over 100 (the line); blue under 60: cold | from 108 |
| IAT | over 50 | from 65 |

- Boost is MAP minus the EMU's barometer (BARO, base+2); until that
  comes, against 101.3 kPa.
- AFR is lambda x 14.7, petrol (`EMU_STOICH` in `main/ui_emu.h`).
- The EMU's error flags (base+4): MAP, wideband, CLT or IAT sensor failed
  puts SENSOR ERR where the value was.
- A channel whose frame is older than 1 s shows `--`.
- Limits, ranges and scale lines: the `CH[]` table in `main/ui_emu.c`. The
  background: `tools/gen_bg.py` -> `main/emu_bg.c`.

## CAN

EMU Black: CAN-Bus -> EMU CAN stream on, base ID 0x600 (the default;
another one: `EMU_BASE_ID` in `main/emu_stream.h`). The stream is eight
frames, 11-bit IDs, little-endian, 20 Hz; `main/emu_stream.c` has the
table.

The bit rate is whatever the EMU is set to (125 k, 250 k, 500 k or 1 M):
the gauge finds it by listening at each rate in turn (listen-only, so a
wrong rate does nothing to the bus), then switches to normal mode at the
right one so it acknowledges the frames even when it is the EMU's only
partner on the bus. It never sends a frame. The rate found is stored and
tried first at the next start; if the stream goes quiet for 3 s it looks
again. The log says `EMU stream at N kbit`.

Wiring as on the OBD gauge: the SN65HVD230 board on GPIO20 (TX) / GPIO19
(RX), CAN-H / CAN-L to the EMU's CAN bus. The bus needs 120 R at both
ends; the EMU end is in the EMU harness, a gauge at the far end needs one
too.

## Build and flash

ESP-IDF 5.x, from this folder:

```
idf.py set-target esp32s3
idf.py build flash monitor
```

Flash and monitor over the board's "UART" USB-C. The board drivers, the
boot logo and the splash are `../notstock-round/main` (`hw.c`, `boot_fb.c`,
`splash.c`); the rest is here.

## On a PC

```
make -C tools/sim LVGL_DIR=/path/to/lvgl-8.4
tools/sim/build/sim out.ppm boost=1.45 afr=12.1 tps=64   # page=1 clt= iat=
                                       # fan=1, err=8, link=0
python3 tools/topng.py out.ppm out.png
cc -Imain tools/test/test_stream.c main/emu_stream.c -lm && ./a.out
```

The simulator feeds the page real stream frames through the decoder.

Fonts: Barlow Condensed (SIL OFL, `../notstock-round/assets/fonts`), made
with lv_font_conv: `emu_num_80` / `emu_num_64` the digits, `emu_txt_28` /
`emu_txt_18` labels, units and scales.
