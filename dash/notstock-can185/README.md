# NOT STOCK CAN 1.85

A round board that sits behind the **Waveshare ESP32-S3-Touch-LCD-1.85**
(SKU 28514) on its three M2 holes and turns the car's OBD supply and CAN
into what the round gauge needs. One 4-wire cable from the OBD plug; two
short cables to the display.

![top](docs/top.png)

KiCad 7 project: `notstock-can185.kicad_pro` (schematic, PCB). Schematic as
PDF: [docs/schematic.pdf](docs/schematic.pdf). Placement:
[docs/assembly.png](docs/assembly.png).

**State: rev 3, designed, DRC clean (0 errors, 0 unconnected), ready to
order with assembly; not built yet.**

## How it connects

The display has no plug-on header. What it has, and what this board uses
(Waveshare's schematic and drawing of the board, `ESP32-S3-LCD-1.85`):

| Display | Cable | This board |
| --- | --- | --- |
| USB-C (5 V in) | short USB-C to USB-C, angled ends | J3 USB-C (HRO TYPE-C-31-M-12), 5 V out |
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
| 5 V out | J3 USB-C (data pins open), 56 k on CC1/CC2: a plain 5 V source |
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

## Make it (JLCPCB, with assembly)

Three files in `fab/`:

| File | Where on JLCPCB |
| --- | --- |
| `notstock-can185-gerbers.zip` | "Add gerber file": 2 layers, 1.6 mm, any colour |
| `bom.csv` | PCB Assembly, "Add BOM File" |
| `cpl.csv` | PCB Assembly, "Add CPL File" |

Assembly: top side only, all SMD. Every part has its LCSC number in
`bom.csv` (also in the schematic and on the footprints, field "LCSC"):

| Ref | Part | LCSC |
| --- | --- | --- |
| U1 | TI LMR16006YDDCR | C290195 |
| U2 | NXP TJA1051T/3/1J | C38695 |
| D4 | onsemi NUP2105LT1G | C14486 |
| D1 | SS16 (UMW) | C2758574 |
| D2 | SMAJ26A | C383018 |
| D3 | Nexperia PMEG6010CEH | C110797 |
| F1 | SMD1812P050TF/30 (RUILON) | C12559 |
| L1 | Sunlord SWPA4026S220MT | C88254 |
| J1 | JST B4B-PH-SM4-TB | C160354 |
| J2 | JST BM04B-SRSS-TB | C160390 |
| J3 | HRO TYPE-C-31-M-12 | C165948 |
| C1 | 4.7 uF 50 V 1206 (Murata) | C77096 |
| C2, C3, C6, C7 | 100 nF 50 V 0603 (YAGEO) | C14663 |
| C4, C5 | 22 uF 16 V 1206 (Samsung) | C90146 |
| C8 | 4.7 nF 50 V 0603 | C53987 |
| R1 | 100 k 0603 | C25803 |
| R2, R7, R8 | 56 k 0603 | C23206 |
| R3 | 10 k 0603 | C25804 |
| R6 | 1 k 0603 | C21190 |
| R4, R5 | 62 R 0805 1 % (Walsin MR08X62R0FTL) | C5805111 |

R4/R5: any 0805 1 % from 56 to 62 R will do (the bus wants 2 x 60 R). With
JP1 open they do nothing; if no such part is in stock, R4, R5 and C8 can be
left off ("Do not place" in the BOM step).

JP1 is a solder bridge, not a part. Stock changes: JLCPCB marks a part out
of stock in the BOM step; pick an equivalent there (same value, package,
voltage). The passives are JLCPCB basic parts, the rest extended (a small
loading fee each).

In the placement preview, check the parts with a direction: U1 (pin 1 dot),
U2 (pin 1 at the notch), D4, D1/D2/D3 (the band towards the "K" side of the
footprint's silkscreen), J1, J2, J3 (openings: J1 and J2 upwards, J3 to the
board edge). KiCad and JLCPCB do not always agree on rotations; turn any
that are off in 90° steps there.

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
