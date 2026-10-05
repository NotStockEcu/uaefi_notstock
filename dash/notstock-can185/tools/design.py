"""NOT STOCK CAN 1.85: the parts, their nets and where they go.

One source for both generators: gen_sch.py draws the schematic from it,
gen_pcb.py places the footprints and gives their pads the same nets, so
the two always agree. Coordinates in mm, board centre at (0, 0), y down,
seen from the back of the display (this board's component side).

The board plugs onto the 28-pin 1.27 mm header of the Waveshare
ESP32-S3-Touch-LCD-1.85 and sits behind it on the display's three M2
holes. Mechanics and header pins from Waveshare's drawing and schematic of
the ESP32-S3-Touch-LCD-1.85C (hardware/ in their repository): header
2 x 14 at 16.3 mm from the centre, holes on a 23.75 mm radius.
"""

# ----------------------------------------------------------------- board
BOARD_R = 26.5            # outline radius
HOLES = [(-14.0, -19.18), (14.0, -19.18), (0.0, 23.75)]   # M2

# the display's header, rows along x. Which end is pin 1: the 1.85C
# schematic and drawing do not tell. Set here after a look at the board's
# silkscreen, then regenerate. A 2 x 14 header numbered the usual way
# (odd/even across, pin 2 beside pin 1) has, seen from the back, pin 1 on
# the inner row when it is at the left end and on the outer row when it is
# at the right: the other two combinations would be a mirrored header.
HDR_Y = -16.3             # centre between the two rows
PIN1_END = "left"         # "left" or "right" (seen from the back)
ODD_ROW = "inner"         # follows from PIN1_END for a usual header

# ----------------------------------------------------------------- parts
# ref: (lib symbol, value, footprint, {pin: net}, (x, y, rot), side, mpn)
# the side is where the footprint goes: F (component side) or B
P = {}


def part(ref, sym, val, fp, pins, at, side="F", mpn="", lcsc=""):
    P[ref] = dict(sym=sym, val=val, fp=fp, pins=pins, at=at, side=side,
                  mpn=mpn or val, lcsc=lcsc)


R0603 = "Resistor_SMD:R_0603_1608Metric"
R0805 = "Resistor_SMD:R_0805_2012Metric"
C0603 = "Capacitor_SMD:C_0603_1608Metric"
C1206 = "Capacitor_SMD:C_1206_3216Metric"

# car side: 12 V, GND, CAN-H, CAN-L
# SMD, so nothing pokes through towards the display; the cable leaves
# straight backwards. Mating housing JST PHR-4 with SPH-002T-P0.5S crimps.
part("J1", "Connector_Generic:Conn_01x04", "OBD 12V GND H L",
     "Connector_JST:JST_PH_B4B-PH-SM4-TB_1x04-1MP_P2.00mm_Vertical",
     {"1": "VBAT", "2": "GND", "3": "CANH", "4": "CANL"},
     (0.0, 13.0, 0), mpn="JST B4B-PH-SM4-TB")

# input: resettable fuse, reverse polarity diode, TVS against load dump
part("F1", "Device:Polyfuse", "0.5A 30V", "Fuse:Fuse_1812_4532Metric",
     {"1": "VBAT", "2": "VBAT_F"}, (-10.5, 14.5, 0),
     mpn="Littelfuse 1812L050/30DR")
part("D1", "Device:D_Schottky", "SS16", "Diode_SMD:D_SMA",
     {"1": "VIN", "2": "VBAT_F"}, (-17.0, 8.5, 270), mpn="SS16 (60 V 1 A)")
part("D2", "Diode:SMAJ26A", "SMAJ26A", "Diode_SMD:D_SMA",
     {"1": "VIN", "2": "GND"}, (-21.0, 1.0, 90), mpn="SMAJ26A")
part("C1", "Device:C", "4.7u 50V", C1206, {"1": "VIN", "2": "GND"},
     (-16.6, -1.0, 90), mpn="4.7 uF 50 V X7R 1206")
part("C2", "Device:C", "100n 50V", C0603, {"1": "VIN", "2": "GND"},
     (-13.7, -1.0, 180), mpn="100 nF 50 V X7R 0603")

# 12 V -> 5 V, LMR16006 (60 V, 0.6 A, 700 kHz Y version)
part("U1", "Regulator_Switching:LMR16006YQ", "LMR16006YDDCR",
     "Package_TO_SOT_SMD:SOT-23-6",
     {"1": "CB", "2": "GND", "3": "FB", "4": "EN", "5": "VIN", "6": "SW"},
     (-10.0, -1.0, 180), mpn="TI LMR16006YDDCR")
part("R1", "Device:R", "100k", R0603, {"1": "VIN", "2": "EN"},
     (-13.9, -3.4, 0), mpn="100 k 1% 0603")
part("C3", "Device:C", "100n", C0603, {"1": "CB", "2": "SW"},
     (-10.0, 1.7, 180), mpn="100 nF 25 V X7R 0603")
part("D3", "Device:D_Schottky", "PMEG6010CEH", "Diode_SMD:D_SOD-123F",
     {"1": "SW", "2": "GND"}, (-14.3, 2.5, 180), mpn="Nexperia PMEG6010CEH")
part("L1", "Device:L", "22u", "Inductor_SMD:L_Sunlord_SWPA4026S",
     {"1": "SW", "2": "+5V"}, (-10.5, 6.2, 90),
     mpn="Sunlord SWPA4026S220MT (22 uH, 1 A)")
part("R2", "Device:R", "56k", R0603, {"1": "+5V", "2": "FB"},
     (-6.0, -4.6, 90), mpn="56 k 1% 0603")
part("R3", "Device:R", "10k", R0603, {"1": "FB", "2": "GND"},
     (-7.6, -4.6, 270), mpn="10 k 1% 0603")
part("C4", "Device:C", "22u 10V", C1206, {"1": "+5V", "2": "GND"},
     (-6.6, 4.6, 90), mpn="22 uF 10 V X5R 1206")
part("C5", "Device:C", "22u 10V", C1206, {"1": "+5V", "2": "GND"},
     (-4.0, 4.6, 90), mpn="22 uF 10 V X5R 1206")

# CAN: TJA1051T/3, 5 V core, 3.3 V logic from the display
part("U2", "Interface_CAN_LIN:TJA1051T-3", "TJA1051T/3",
     "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",
     {"1": "TXD", "2": "GND", "3": "+5V", "4": "RXD", "5": "+3V3",
      "6": "CANL", "7": "CANH", "8": "GND"},
     (8.0, -5.0, 0), mpn="NXP TJA1051T/3")
part("C6", "Device:C", "100n", C0603, {"1": "+5V", "2": "GND"},
     (2.6, -4.5, 90), mpn="100 nF 25 V X7R 0603")
part("C7", "Device:C", "100n", C0603, {"1": "+3V3", "2": "GND"},
     (12.6, -10.0, 0), mpn="100 nF 25 V X7R 0603")
part("D4", "Power_Protection:NUP2105L", "NUP2105L", "Package_TO_SOT_SMD:SOT-23",
     {"1": "CANH", "2": "CANL", "3": "GND"}, (15.5, -2.0, 0),
     mpn="onsemi NUP2105LT1G")
# split termination, open by default: the car's bus is terminated already
part("R4", "Device:R", "60R4", R0805, {"1": "CANH", "2": "TMID"},
     (11.0, 5.0, 90), mpn="60.4 R 1% 0805")
part("R5", "Device:R", "60R4", R0805, {"1": "TMID", "2": "TERM"},
     (14.0, 5.0, 90), mpn="60.4 R 1% 0805")
part("C8", "Device:C", "4.7n", C0603, {"1": "TMID", "2": "GND"},
     (17.0, 5.0, 90), mpn="4.7 nF 50 V X7R 0603")
part("JP1", "Jumper:SolderJumper_2_Open", "TERM",
     "Jumper:SolderJumper-2_P1.3mm_Open_Pad1.0x1.5mm",
     {"1": "TERM", "2": "CANL"}, (14.0, 8.4, 270), mpn="(solder bridge)")

# the display: Waveshare's 28-pin header, pins as in the 1.85C schematic
HDR = {"1": "+5V", "3": "GND", "4": "GND", "9": "+3V3", "10": "+3V3",
       "11": "GND", "12": "GND", "18": "RXD", "20": "TXD"}
part("J2", "Connector_Generic:Conn_02x14_Odd_Even", "LCD-1.85 header",
     "Connector_PinSocket_1.27mm:PinSocket_2x14_P1.27mm_Vertical",
     HDR, (0.0, HDR_Y, 0), side="B",
     mpn="2x14 socket 1.27 mm THT, to mate with the display's header")
HDR_NAMES = {"1": "USB_5V", "2": "BAT", "3": "GND", "4": "GND", "5": "D-",
             "6": "SCL", "7": "D+", "8": "SDA", "9": "3V3", "10": "3V3",
             "11": "GND", "12": "GND", "13": "GPIO0", "14": "GPIO43",
             "15": "GPIO1", "16": "GPIO44", "17": "GPIO3", "18": "GPIO13",
             "19": "GPIO4", "20": "GPIO12", "21": "GPIO6", "22": "EXIO8",
             "23": "GPIO7", "24": "EXIO7", "25": "GPIO8", "26": "EXIO6",
             "27": "GPIO9", "28": "EXIO5"}

for i, (x, y) in enumerate(HOLES, 1):
    part("H%d" % i, "Mechanical:MountingHole", "M2",
         "MountingHole:MountingHole_2.2mm_M2", {}, (x, y, 0), mpn="")

# nets that carry current, for the wider tracks
POWER = {"VBAT", "VBAT_F", "VIN", "SW", "+5V", "GND"}
