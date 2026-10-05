"""NOT STOCK CAN 1.85: the parts, their nets and where they go.

One source for both generators: gen_sch.py draws the schematic from it,
gen_pcb.py places the footprints and gives their pads the same nets, so
the two always agree. Coordinates in mm, board centre at (0, 0), y down,
seen from behind the display (this board's component side).

The board sits behind the Waveshare ESP32-S3-Touch-LCD-1.85 on spacers in
the display's three M2 holes. The display has no plug-on header: CAN goes
through its 4-pin 1.0 mm UART socket (J9 on the display: 1 RXD/GPIO44,
2 TXD/GPIO43, 3 3V3, 4 GND) on the 4-wire cable that comes with the
display (free ends soldered here), which also brings the display's 3.3 V
for the transceiver. The display is powered through its
battery socket (MX1.25 2-pin, J1 on the display: 1 BAT, 2 GND) with 3.75 V
on two wires soldered to this board; the display's power button (Key1)
must be bridged, or its battery switch never turns on. Mechanics from Waveshare's drawing of the board
(ESP32-S3-LCD-1.85 structure chart: DXF and STEP), pins from its
schematic.
"""

# ----------------------------------------------------------------- board
BOARD_R = 24.0            # the display's board is 48.08 x 49.95
# the display's M2 holes, from its centre (drawing: 15.73 left, 14.10 up,
# 14.90 down; 21.27 right), y down
HOLES = [(-15.73, -14.10), (-15.73, 14.90), (21.27, 0.0)]

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

# car side: 12 V, GND, CAN-H, CAN-L, K-line. SMD, vertical: the cable
# plugs in straight from behind. Mating housing JST PHR-5 with
# SPH-002T-P0.5S crimps.
part("J1", "Connector_Generic:Conn_01x05", "OBD 12V GND H L K",
     "Connector_JST:JST_PH_B5B-PH-SM4-TB_1x05-1MP_P2.00mm_Vertical",
     {"1": "VBAT", "2": "GND", "3": "CANH", "4": "CANL", "5": "KLINE"},
     (3.0, -15.5, 0), mpn="JST B5B-PH-SM4-TB(LF)(SN)", lcsc="C265085")

# input: resettable fuse, reverse polarity diode, TVS against load dump
part("F1", "Device:Polyfuse", "0.5A 30V", "Fuse:Fuse_1812_4532Metric",
     {"1": "VBAT", "2": "VBAT_F"}, (-8.5, -11.5, 180),
     mpn="RUILON SMD1812P050TF/30 (0.5 A, 30 V)", lcsc="C12559")
part("D1", "Device:D_Schottky", "SS16", "Diode_SMD:D_SMA",
     {"1": "VIN", "2": "VBAT_F"}, (-15.0, -6.5, 90), mpn="UMW SS16 (60 V 1 A)", lcsc="C2758574")
part("D2", "Diode:SMAJ26A", "SMAJ26A", "Diode_SMD:D_SMA",
     {"1": "VIN", "2": "GND"}, (-20.0, 0.0, 90), mpn="SMAJ26A", lcsc="C383018")
part("C1", "Device:C", "4.7u 50V", C1206, {"1": "VIN", "2": "GND"},
     (-16.6, 1.5, 90), mpn="Murata GRM31CR71H475KA12L (4.7 uF 50 V X7R 1206)", lcsc="C77096")
part("C2", "Device:C", "100n 50V", C0603, {"1": "VIN", "2": "GND"},
     (-13.7, 1.5, 180), mpn="YAGEO CC0603KRX7R9BB104 (100 nF 50 V)", lcsc="C14663")

# 12 V -> 3.75 V for the display's battery input, LMR16006 (60 V, 0.6 A,
# 700 kHz Y version)
part("U1", "Regulator_Switching:LMR16006YQ", "LMR16006YDDCR",
     "Package_TO_SOT_SMD:SOT-23-6",
     {"1": "CB", "2": "GND", "3": "FB", "4": "EN", "5": "VIN", "6": "SW"},
     (-10.0, 1.5, 180), mpn="TI LMR16006YDDCR", lcsc="C290195")
# ignition from the K-line (OBD 7): at battery voltage with the key on,
# low with it off. 100 k / 13 k onto EN (1.21 V rising, 1.07 V falling):
# on above 10.5 V, off below 9.3 V. 100 nF against K-line traffic.
part("R1", "Device:R", "100k", R0603, {"1": "KLINE", "2": "EN"},
     (-13.9, -0.9, 0), mpn="UNI-ROYAL 0603WAF1003T5E (100 k 1%)", lcsc="C25803")
part("R9", "Device:R", "13k", R0603, {"1": "EN", "2": "GND"},
     (-4.0, -5.0, 90), mpn="UNI-ROYAL 0603WAF1302T5E (13 k 1%)", lcsc="C22797")
part("C9", "Device:C", "100n", C0603, {"1": "EN", "2": "GND"},
     (-2.4, -5.0, 90), mpn="YAGEO CC0603KRX7R9BB104 (100 nF 50 V)", lcsc="C14663")
part("C3", "Device:C", "100n", C0603, {"1": "CB", "2": "SW"},
     (-10.0, 4.2, 180), mpn="YAGEO CC0603KRX7R9BB104 (100 nF 50 V)", lcsc="C14663")
part("D3", "Device:D_Schottky", "PMEG6010CEH", "Diode_SMD:D_SOD-123F",
     {"1": "SW", "2": "GND"}, (-14.3, 5.0, 180), mpn="Nexperia PMEG6010CEH,115", lcsc="C110797")
part("L1", "Device:L", "22u", "Inductor_SMD:L_Sunlord_SWPA4026S",
     {"1": "SW", "2": "BAT_LCD"}, (-10.5, 8.7, 90),
     mpn="Sunlord SWPA4026S220MT (22 uH)", lcsc="C88254")
# 0.765 V x (1 + 39 k / 10 k) = 3.75 V
part("R2", "Device:R", "39k", R0603, {"1": "BAT_LCD", "2": "FB"},
     (-6.0, -2.1, 90), mpn="UNI-ROYAL 0603WAF3902T5E (39 k 1%)", lcsc="C23153")
part("R3", "Device:R", "10k", R0603, {"1": "FB", "2": "GND"},
     (-7.6, -2.1, 270), mpn="UNI-ROYAL 0603WAF1002T5E (10 k 1%)", lcsc="C25804")
part("C4", "Device:C", "22u 16V", C1206, {"1": "BAT_LCD", "2": "GND"},
     (-6.6, 7.1, 90), mpn="Samsung CL31A226KOHNNNE (22 uF 16 V X5R 1206)", lcsc="C90146")
part("C5", "Device:C", "22u 16V", C1206, {"1": "BAT_LCD", "2": "GND"},
     (-4.0, 7.1, 90), mpn="Samsung CL31A226KOHNNNE (22 uF 16 V X5R 1206)", lcsc="C90146")

# 3.75 V out to the display's battery socket: two wires, soldered here,
# with an MX1.25 2-pin plug at the display. Not assembled (pads only).
part("J3", "Connector_Generic:Conn_01x02", "BAT to display",
     "Connector_Wire:SolderWire-0.25sqmm_1x02_P4.2mm_D0.65mm_OD1.7mm",
     {"1": "BAT_LCD", "2": "GND"}, (-2.1, 19.0, 0), mpn="(wires)", dnp=True)

# CAN: SN65HVD230 on the display's 3.3 V (UART socket pin 3); Rs to GND:
# full speed, no slope control
part("U2", "Interface_CAN_LIN:SN65HVD230", "SN65HVD230",
     "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",
     {"1": "TXD", "2": "GND", "3": "+3V3", "4": "RXD_T",
      "6": "CANL", "7": "CANH", "8": "GND"},
     (8.0, -3.0, 0), mpn="TI SN65HVD230DR", lcsc="C12084")
part("C6", "Device:C", "100n", C0603, {"1": "+3V3", "2": "GND"},
     (3.0, -3.0, 90), mpn="YAGEO CC0603KRX7R9BB104 (100 nF 50 V)", lcsc="C14663")
part("C7", "Device:C", "100n", C0603, {"1": "+3V3", "2": "GND"},
     (12.0, -8.0, 0), mpn="YAGEO CC0603KRX7R9BB104 (100 nF 50 V)", lcsc="C14663")
part("D4", "Power_Protection:NUP2105L", "NUP2105L", "Package_TO_SOT_SMD:SOT-23",
     {"1": "CANH", "2": "CANL", "3": "GND"}, (15.5, -3.5, 0),
     mpn="onsemi NUP2105LT1G", lcsc="C14486")
# split termination, open by default: the car's bus is terminated already.
# Not assembled (dnp): only for a bench, soldered by hand then; any 0805
# 56..62 R 1 %
part("R4", "Device:R", "62R", R0805, {"1": "CANH", "2": "TMID"},
     (11.0, 4.5, 90), mpn="62 R 1% 0805", dnp=True)
part("R5", "Device:R", "62R", R0805, {"1": "TMID", "2": "TERM"},
     (14.0, 4.5, 90), mpn="62 R 1% 0805", dnp=True)
part("C8", "Device:C", "4.7n", C0603, {"1": "TMID", "2": "GND"},
     (16.8, 4.5, 90), mpn="FH 0603B472K500NT (4.7 nF 50 V X7R)", lcsc="C53987", dnp=True)
part("JP1", "Jumper:SolderJumper_2_Open", "TERM",
     "Jumper:SolderJumper-2_P1.3mm_Open_Pad1.0x1.5mm",
     {"1": "TERM", "2": "CANL"}, (14.0, 8.4, 270), mpn="(solder bridge)")

# the display's UART socket, through the 4-wire cable that comes with the
# display (SH plug at the display, free ends soldered here). GPIO43 (the
# display's TXD) is also where the boot ROM prints its messages for a
# moment after reset: 1 k keeps that from fighting the transceiver's RXD.
# Pads only, not assembled. The footprint's pad 1 is its origin.
part("J2", "Connector_Generic:Conn_01x04", "UART wires from display",
     "Connector_Wire:SolderWire-0.15sqmm_1x04_P4mm_D0.5mm_OD1.5mm",
     {"1": "TXD", "2": "RXD", "3": "+3V3", "4": "GND"},
     (6.0, 12.0, 0), mpn="(wires)", dnp=True)
part("R6", "Device:R", "1k", R0603, {"1": "RXD", "2": "RXD_T"},
     (5.6, 16.8, 90), mpn="UNI-ROYAL 0603WAF1001T5E (1 k 1%)", lcsc="C21190")

for i, (x, y) in enumerate(HOLES, 1):
    part("H%d" % i, "Mechanical:MountingHole", "M2",
         "MountingHole:MountingHole_2.2mm_M2", {}, (x, y, 0), mpn="")

# nets that carry current, for the wider tracks
POWER = {"VBAT", "VBAT_F", "VIN", "SW", "BAT_LCD", "GND"}
