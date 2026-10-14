"""NOT STOCK CAN 1.32: the parts, their nets and where they go.

One source for both generators: gen_sch.py draws the schematic from it,
gen_pcb.py places the footprints and gives their pads the same nets, so
the two always agree. Coordinates in mm, board centre at (0, 0), y down,
seen from behind the display (this board's component side): the display's
12-pin connector (J1 on the display) at the top, its USB-C at the bottom.

The board sits behind the Waveshare ESP32-S3-Touch-AMOLED-1.32 on its
three standoffs (M2). Everything goes through the display's 12-pin
connector (side entry, 1.0 mm, on the top edge; Waveshare's schematic:
1 GND, 2 VSYS, 3 VCC3V3, 4 GP0, 5 GP1, 6 GP2, 7 SCL, 8 SDA, 9 D-, 10 D+,
11 TXD, 12 RXD) on the cable that comes with the display, free ends
soldered here: 5 V into VSYS (the system rail behind the display's USB
diode, 2.3..5.5 V into its 3.3 V regulator: the display runs from it with
no battery and no power button), the display's 3.3 V for the
transceiver, CAN on GP1 (TX) and GP2 (RX). No battery on the display.
Mechanics from Waveshare's STEP of the board, pins from its schematic.

The display's parts under this board reach 0.25 mm short of the standoffs'
ends (USB-C, the 12-pin connector): nothing on this board's display side.
"""

# ----------------------------------------------------------------- board
BOARD_R = 17.2            # the display is 36.4 across
# the display's standoffs, from its centre, seen from behind (y down):
# Waveshare's STEP has them at (15.87, 0) and (-10.8, +-11.04) seen from
# the front
HOLES = [(-15.87, 0.0), (10.8, -11.04), (10.8, 11.04)]

# ----------------------------------------------------------------- parts
# ref: (lib symbol, value, footprint, {pin: net}, (x, y, rot), side, mpn)
# the side is where the footprint goes: F (component side) or B
P = {}


def part(ref, sym, val, fp, pins, at, side="F", mpn="", lcsc="", dnp=False):
    P[ref] = dict(sym=sym, val=val, fp=fp, pins=pins, at=at, side=side,
                  mpn=mpn or val, lcsc=lcsc, dnp=dnp)


R0603 = "Resistor_SMD:R_0603_1608Metric"
R0805 = "Resistor_SMD:R_0805_2012Metric"
C0603 = "Capacitor_SMD:C_0603_1608Metric"
C1206 = "Capacitor_SMD:C_1206_3216Metric"

# car side: 12 V, GND, CAN-H, CAN-L. The 12 V is OBD pin 1, terminal 15 on
# VW (+12 V switched by the key, as the CANcheck display takes it): the
# board is on with the key and off without it. SMD, vertical: the cable
# plugs in straight from behind. Mating housing JST PHR-4 with
# SPH-002T-P0.5S crimps.
part("J1", "Connector_Generic:Conn_01x04", "OBD 12V GND H L",
     "Connector_JST:JST_PH_B4B-PH-SM4-TB_1x04-1MP_P2.00mm_Vertical",
     {"1": "VBAT", "2": "GND", "3": "CANH", "4": "CANL"},
     (-1.0, 10.5, 0), mpn="JST B4B-PH-SM4-TB(LF)(SN)", lcsc="C160354")

# input: resettable fuse, reverse polarity diode, TVS against load dump
part("F1", "Device:Polyfuse", "0.5A 30V", "Fuse:Fuse_1812_4532Metric",
     {"1": "VBAT", "2": "VBAT_F"}, (-11.5, 4.0, 90),
     mpn="RUILON SMD1812P050TF/30 (0.5 A, 30 V)", lcsc="C12559")
part("D1", "Device:D_Schottky", "SS16", "Diode_SMD:D_SMA",
     {"1": "VIN", "2": "VBAT_F"}, (-11.5, -4.0, 90), mpn="UMW SS16 (60 V 1 A)", lcsc="C2758574")
part("D2", "Diode:SMAJ26A", "SMAJ26A", "Diode_SMD:D_SMA",
     {"1": "VIN", "2": "GND"}, (-6.2, -8.85, 0), mpn="SMAJ26A", lcsc="C383018")
part("C1", "Device:C", "4.7u 50V", C1206, {"1": "VIN", "2": "GND"},
     (-7.3, -5.6, 0), mpn="Murata GRM31CR71H475KA12L (4.7 uF 50 V X7R 1206)", lcsc="C77096")
part("C2", "Device:C", "100n 50V", C0603, {"1": "VIN", "2": "GND"},
     (-3.5, -5.6, 0), mpn="YAGEO CC0603KRX7R9BB104 (100 nF 50 V)", lcsc="C14663")

# 12 V -> 5 V for the display's VSYS, LMR16006 (60 V, 0.6 A, 2.1 MHz Y
# version; the X version is 0.7 MHz). At 2.1 MHz the 80 ns minimum on-time
# allows 5 V from up to about 30 V before the chip folds its frequency
# back; 22 uH gives about 70 mA ripple. Under about 5.5 V in (cranking)
# the output follows the input down; the display's regulator runs from
# 2.3 V.
part("U1", "Regulator_Switching:LMR16006YQ", "LMR16006YDDCR",
     "Package_TO_SOT_SMD:SOT-23-6",
     {"1": "CB", "2": "GND", "3": "FB", "4": "EN", "5": "VIN", "6": "SW"},
     (-6.0, -1.8, 0), mpn="TI LMR16006YDDCR", lcsc="C290195")
part("R1", "Device:R", "100k", R0603, {"1": "VIN", "2": "EN"},
     (-2.3, -2.5, 90), mpn="UNI-ROYAL 0603WAF1003T5E (100 k 1%)", lcsc="C25803")
part("C3", "Device:C", "100n", C0603, {"1": "CB", "2": "SW"},
     (-6.0, 1.0, 0), mpn="YAGEO CC0603KRX7R9BB104 (100 nF 50 V)", lcsc="C14663")
# K16 (60 V 1 A, SOD-123FL, the same pads as SOD-123F); the PMEG6010CEH
# (C110797) went out of stock at LCSC
part("D3", "Device:D_Schottky", "K16", "Diode_SMD:D_SOD-123F",
     {"1": "SW", "2": "GND"}, (-6.5, 3.5, 0), mpn="GOODWORK K16 (60 V 1 A Schottky, SOD-123FL)", lcsc="C2943756")
part("L1", "Device:L", "22u", "Inductor_SMD:L_Sunlord_SWPA4026S",
     {"1": "SW", "2": "VSYS"}, (-1.5, 2.5, 0),
     mpn="Sunlord SWPA4026S220MT (22 uH)", lcsc="C88254")
# 0.765 V x (1 + 56 k / 10 k) = 5.05 V
part("R2", "Device:R", "56k", R0603, {"1": "VSYS", "2": "FB"},
     (-0.5, -2.0, 90), mpn="UNI-ROYAL 0603WAF5602T5E (56 k 1%)", lcsc="C23206")
part("R3", "Device:R", "10k", R0603, {"1": "FB", "2": "GND"},
     (1.2, -2.0, 270), mpn="UNI-ROYAL 0603WAF1002T5E (10 k 1%)", lcsc="C25804")
part("C4", "Device:C", "22u 16V", C1206, {"1": "VSYS", "2": "GND"},
     (3.5, 1.0, 90), mpn="Samsung CL31A226KOHNNNE (22 uF 16 V X5R 1206)", lcsc="C90146")
part("C5", "Device:C", "22u 16V", C1206, {"1": "VSYS", "2": "GND"},
     (5.9, 1.0, 90), mpn="Samsung CL31A226KOHNNNE (22 uF 16 V X5R 1206)", lcsc="C90146")

# CAN: SN65HVD230 on the display's 3.3 V (UART socket pin 3); Rs to GND:
# full speed, no slope control
part("U2", "Interface_CAN_LIN:SN65HVD230", "SN65HVD230",
     "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",
     {"1": "TXD", "2": "GND", "3": "+3V3", "4": "RXD_T",
      "6": "CANL", "7": "CANH", "8": "GND"},
     (5.0, -5.5, 90), mpn="TI SN65HVD230DR", lcsc="C12084")
part("C6", "Device:C", "100n", C0603, {"1": "+3V3", "2": "GND"},
     (1.0, -7.2, 90), mpn="YAGEO CC0603KRX7R9BB104 (100 nF 50 V)", lcsc="C14663")
part("C7", "Device:C", "100n", C0603, {"1": "+3V3", "2": "GND"},
     (12.5, 0.0, 90), mpn="YAGEO CC0603KRX7R9BB104 (100 nF 50 V)", lcsc="C14663")
part("D4", "Power_Protection:NUP2105L", "NUP2105L", "Package_TO_SOT_SMD:SOT-23",
     {"1": "CANH", "2": "CANL", "3": "GND"}, (10.0, -4.0, 0),
     mpn="onsemi NUP2105LT1G", lcsc="C14486")
# split termination, open by default: the car's bus is terminated already.
# Not assembled (dnp): only for a bench, soldered by hand then; any 0805
# 56..62 R 1 %
part("R4", "Device:R", "62R", R0805, {"1": "CANH", "2": "TMID"},
     (8.5, 2.5, 90), mpn="62 R 1% 0805", dnp=True)
part("R5", "Device:R", "62R", R0805, {"1": "TMID", "2": "TERM"},
     (11.0, 3.5, 90), mpn="62 R 1% 0805", dnp=True)
part("C8", "Device:C", "4.7n", C0603, {"1": "TMID", "2": "GND"},
     (13.5, 3.5, 90), mpn="FH 0603B472K500NT (4.7 nF 50 V X7R)", lcsc="C53987", dnp=True)
part("JP1", "Jumper:SolderJumper_2_Open", "TERM",
     "Jumper:SolderJumper-2_P1.3mm_Open_Pad1.0x1.5mm",
     {"1": "TERM", "2": "CANL"}, (8.5, 6.5, 0), mpn="(solder bridge)")

# the display's 12-pin connector, through the cable that comes with the
# display (the plug at the display, free ends soldered here): its pins 1,
# 2, 3, 5 and 6; the rest of the wires cut off. GP2 is a strapping-free
# pin, but 1 k still keeps the transceiver's RXD and the GPIO from fighting
# if the firmware drives it. Pads only, not assembled; pad 1 is the
# footprint's origin.
part("J2", "Connector_Generic:Conn_01x05", "wires from the display",
     "Connector_Wire:SolderWire-0.127sqmm_1x05_P3.7mm_D0.48mm_OD1mm",
     {"1": "GND", "2": "VSYS", "3": "+3V3", "4": "TXD", "5": "RXD"},
     (-8.2, -12.0, 0), mpn="(wires)", dnp=True)
part("R6", "Device:R", "1k", R0603, {"1": "RXD", "2": "RXD_T"},
     (0.6, -9.7, 0), mpn="UNI-ROYAL 0603WAF1001T5E (1 k 1%)", lcsc="C21190")

for i, (x, y) in enumerate(HOLES, 1):
    part("H%d" % i, "Mechanical:MountingHole", "M2",
         "MountingHole:MountingHole_2.2mm_M2", {}, (x, y, 0), mpn="")

# nets that carry current, for the wider tracks
POWER = {"VBAT", "VBAT_F", "VIN", "SW", "VSYS", "GND"}
