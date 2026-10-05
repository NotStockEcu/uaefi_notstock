# NOT STOCK CAN 1.85

A round board that sits behind the **Waveshare ESP32-S3-Touch-LCD-1.85**
(SKU 28514) on its three M2 holes and turns the car's OBD supply and CAN
into what the round gauge needs. One 4-wire cable from the OBD plug; two
short cables to the display.

![top](docs/top.png)

KiCad 7 project: `notstock-can185.kicad_pro` (schematic, PCB). Schematic as
PDF: [docs/schematic.pdf](docs/schematic.pdf). Placement:
[docs/assembly.png](docs/assembly.png).

**State: rev 2, designed, DRC clean (0 errors, 0 unconnected), not built.**

## How it connects

The display has no plug-on header. What it has, and what this board uses
(Waveshare's schematic and drawing of the board, `ESP32-S3-LCD-1.85`):

| Display | Cable | This board |
| --- | --- | --- |
| USB-C (5 V in) | short USB-C to USB-C, angled ends | J3 USB-C, 5 V out |
| UART socket, 4-pin 1.0 mm, bottom right (1 RXD/GPIO44, 2 TXD/GPIO43, 3 3V3, 4 GND) | JST SH 4-pin, 1:1 (Qwiic/STEMMA QT style), about 5 cm | J2 JST SH 4-pin |
| three M2 holes (15.73 left, 14.10 up / 14.90 down; 21.27 right) | M2 spacers, 6 to 8 mm | H1..H3 |

The car: J1, JST PH 4-pin (12V, GND, CAN-H, CAN-L, marked on the board).

GPIO44 is CAN TX, GPIO43 CAN RX: `../notstock-round-lcd185` is set up for
it. The boot ROM prints on GPIO43 for a moment after every reset; R6 (1 k)
keeps that from fighting the transceiver's RXD. Flashing: unplug the USB-C
cable from J3 and plug the computer into the display instead.

## What is on it

| Block | Parts |
| --- | --- |
| Input | J1 JST PH 4-pin SMD; F1 0.5 A resettable fuse; D1 SS16 against reverse polarity; D2 SMAJ26A against load dump |
| 12 V -> 5 V | U1 LMR16006YDDCR (60 V, 0.6 A, 700 kHz), L1 22 uH, D3 PMEG6010CEH, 2 x 22 uF out; 56 k / 10 k sets 5.05 V |
| 5 V out | J3 USB-C (power only), 56 k on CC1/CC2: a plain 5 V source |
| CAN | U2 TJA1051T/3 (5 V supply, 3.3 V logic from the display's socket); D4 NUP2105L ESD; R4/R5/C8 split termination behind JP1, open; R6 1 k in RXD |
| Mechanics | Ø 48 mm like the display, the display's three M2 holes |

Termination: the car's bus is terminated at both ends already, so JP1
stays open. Bridge it only on a bench with no other terminator.

## Cable to the car

JST PHR-4 housing with SPH-002T-P0.5S crimps on the board side; at the OBD
plug: pin 16 +12 V, pin 4 or 5 GND, pin 6 CAN-H, pin 14 CAN-L. CAN-H and
CAN-L twisted.

## Check before ordering

- The SH cable is 1:1: pin 1 to pin 1. Some ready-made SH cables are
  crossed (reversed); check with a meter: display pin 3 (3V3) must reach J2
  pin 3.
- Height: the spacers must clear the display's tallest parts (USB-C, SD
  slot, the two 1.0 mm sockets, about 4.5 mm) plus the cables' plugs.
- Waveshare's drawing was the source of the outline and the holes; hold
  the printed board (1:1 PDF of `docs/top.svg`) against the display first.

## Make it

PCB: `fab/notstock-can185-gerbers.zip` (2 layers, 1.6 mm) to JLCPCB or
similar. Assembly: `fab/bom.csv` and `fab/cpl.csv` are in JLCPCB's columns;
the LCSC numbers are to be filled in when ordering (stock changes), and
check the part rotations in their preview. All parts on the top side.

## Regenerate

Everything is generated from `tools/design.py` (parts, nets, positions):

```
python3 tools/gen_sch.py                       # the schematic
kicad-cli sch export netlist -o /tmp/n.net notstock-can185.kicad_sch
python3 tools/check_net.py /tmp/n.net          # schematic = design.py
python3 tools/gen_pcb.py --freerouting freerouting-2.0.1.jar
python3 tools/drc_summary.py                   # DRC of the routed board
python3 tools/gen_fab.py                       # fab/ and docs/
```

Needs KiCad 7 with its python module and libraries, Java 21 and
[Freerouting](https://github.com/freerouting/freerouting) 2.x for the
routing. Freerouting is not deterministic: each run gives a different (and
DRC-checked) routing. Opened in KiCad, the project behaves like any other;
the generators overwrite manual changes, so after editing by hand stop
using them.
