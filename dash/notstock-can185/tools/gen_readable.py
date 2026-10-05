"""A readable schematic of the CAN 1.85 board, drawn with wires.

KiCad's schematic (gen_sch.py) joins pins by net labels only; this one is
for reading. Same parts and nets as design.py (checked below), drawn by
hand coordinates. Writes docs/readable.svg.
"""
import math
import os
from design import P

OUT = os.path.join(os.path.dirname(__file__), "..", "docs", "readable.svg")
S = []
INK, DNP, NET = "#111", "#888", "#1565c0"


def line(pts, dash=False, c=INK):
    d = " ".join("%s%g,%g" % ("M" if i == 0 else "L", x, y)
                 for i, (x, y) in enumerate(pts))
    S.append('<path d="%s" fill="none" stroke="%s" stroke-width="2"%s/>'
             % (d, c, ' stroke-dasharray="6 4"' if dash else ""))


def text(x, y, t, size=14, anchor="start", c=INK, bold=False, it=False):
    S.append('<text x="%g" y="%g" font-size="%g" text-anchor="%s" fill="%s"%s%s>%s</text>'
             % (x, y, size, anchor, c, ' font-weight="bold"' if bold else "",
                ' font-style="italic"' if it else "", t))


def dot(x, y):
    S.append('<circle cx="%g" cy="%g" r="4" fill="%s"/>' % (x, y, INK))


def gnd(x, y):
    line([(x, y), (x, y + 12)])
    for i, w in enumerate((12, 8, 4)):
        line([(x - w, y + 12 + 4 * i), (x + w, y + 12 + 4 * i)])


def _axis(x1, y1, x2, y2):
    L = math.hypot(x2 - x1, y2 - y1)
    ux, uy = (x2 - x1) / L, (y2 - y1) / L
    return L, ux, uy, (x1 + x2) / 2, (y1 + y2) / 2


def res(x1, y1, x2, y2, ref, val, dnp=False, side=1):
    L, ux, uy, mx, my = _axis(x1, y1, x2, y2)
    c = DNP if dnp else INK
    line([(x1, y1), (mx - 20 * ux, my - 20 * uy)], dnp, c)
    line([(mx + 20 * ux, my + 20 * uy), (x2, y2)], dnp, c)
    w, h = (40, 14) if uy == 0 else (14, 40)
    S.append('<rect x="%g" y="%g" width="%g" height="%g" fill="#fff" stroke="%s" stroke-width="2"%s/>'
             % (mx - w / 2, my - h / 2, w, h, c, ' stroke-dasharray="6 4"' if dnp else ""))
    label(mx, my, uy, ref, val, side, c)


def label(mx, my, vertical, ref, val, side, c=INK):
    if vertical:
        a = "start" if side > 0 else "end"
        text(mx + 14 * side, my - 3, ref, 13, a, c, True)
        text(mx + 14 * side, my + 13, val, 12, a, c)
    else:
        text(mx, my - 14 if side > 0 else my + 26, ref + "  " + val, 12, "middle", c)


def cap(x1, y1, x2, y2, ref, val, dnp=False, side=1):
    L, ux, uy, mx, my = _axis(x1, y1, x2, y2)
    c = DNP if dnp else INK
    line([(x1, y1), (mx - 4 * ux, my - 4 * uy)], dnp, c)
    line([(mx + 4 * ux, my + 4 * uy), (x2, y2)], dnp, c)
    px, py = -uy, ux
    for k in (-4, 4):
        line([(mx + k * ux - 12 * px, my + k * uy - 12 * py),
              (mx + k * ux + 12 * px, my + k * uy + 12 * py)], False, c)
    label(mx, my, uy, ref, val, side, c)


def diode(xa, ya, xk, yk, ref, val, side=1):
    """anode at (xa, ya), cathode at (xk, yk)"""
    L, ux, uy, mx, my = _axis(xa, ya, xk, yk)
    line([(xa, ya), (mx - 10 * ux, my - 10 * uy)])
    line([(mx + 10 * ux, my + 10 * uy), (xk, yk)])
    px, py = -uy, ux
    b = (mx - 10 * ux, my - 10 * uy)
    t = (mx + 10 * ux, my + 10 * uy)
    S.append('<path d="M%g,%g L%g,%g L%g,%g Z" fill="#fff" stroke="%s" stroke-width="2"/>'
             % (b[0] + 10 * px, b[1] + 10 * py, b[0] - 10 * px, b[1] - 10 * py,
                t[0], t[1], INK))
    line([(t[0] + 10 * px, t[1] + 10 * py), (t[0] - 10 * px, t[1] - 10 * py)])
    label(mx, my, uy, ref, val, side)


def coil(x1, y, x2, ref, val):
    n, w = 4, 10
    mx = (x1 + x2) / 2
    s = mx - n * w
    line([(x1, y), (s, y)])
    line([(s + 2 * n * w, y), (x2, y)])
    d = "M%g,%g " % (s, y) + " ".join("a%g,%g 0 0 1 %g,0" % (w, w, 2 * w) for _ in range(n))
    S.append('<path d="%s" fill="none" stroke="%s" stroke-width="2"/>' % (d, INK))
    text(mx, y - 18, ref + "  " + val, 12, "middle")


def box(x, y, w, h, ref, val, pins, lab=None):
    """pins: (px, py, name, number, side) side L/R/T/B for the text"""
    S.append('<rect x="%g" y="%g" width="%g" height="%g" fill="#fff8e1" stroke="%s" stroke-width="2"/>'
             % (x, y, w, h, INK))
    lx, ly = lab or (x + w / 2, y + h / 2)
    text(lx, ly - 2, ref, 14, "middle", INK, True)
    text(lx, ly + 15, val, 11, "middle")
    for px, py, name, num, side in pins:
        if side == "L":
            text(px + 6, py + 4, name, 11)
            text(px - 4, py - 4, num, 10, "end", "#666")
        elif side == "R":
            text(px - 6, py + 4, name, 11, "end")
            text(px + 4, py - 4, num, 10, "start", "#666")
        elif side == "T":
            text(px, py + 14, name, 11, "middle")
            text(px + 4, py - 4, num, 10, "start", "#666")
        else:
            text(px, py - 6, name, 11, "middle")
            text(px + 4, py + 12, num, 10, "start", "#666")


def net(x, y, t, anchor="start"):
    text(x, y, t, 12, anchor, NET, it=True)


# --------------------------------------------------------------- input
text(30, 34, "NOT STOCK CAN 1.85: schematic with wires (rev 4 / rev 6)", 22, bold=True)
text(30, 56, "Grey dashed: not assembled. Same parts and nets as the KiCad project.", 13, c="#555")

box(30, 90, 80, 140, "J1", "PH 4", lab=(52, 225), pins=[(110, 110, "12V", "1", "R"), (110, 140, "GND", "2", "R"),
                                    (110, 170, "CANH", "3", "R"), (110, 200, "CANL", "4", "R")])
text(70, 250, "to OBD: 1, 4, 6, 14", 11, "middle", "#555")
line([(110, 140), (130, 140)]); gnd(130, 140)
line([(110, 110), (150, 110)])
res(150, 110, 230, 110, "F1", "0.5 A PTC", side=-1)
net(150, 100, "VBAT")
diode(230, 110, 300, 110, "D1", "SS16", side=-1)
net(232, 100, "VBAT_F")
line([(300, 110), (680, 110)])
net(560, 100, "VIN")
for x in (340, 410, 480, 540):
    dot(x, 110)
diode(340, 190, 340, 110, "D2", "SMAJ26A", side=-1); gnd(340, 190)
cap(410, 110, 410, 190, "C1", "4u7"); gnd(410, 190)
cap(480, 110, 480, 190, "C2", "100n"); gnd(480, 190)
res(540, 110, 540, 200, "R1", "100k")
line([(540, 200), (700, 200)])
net(600, 194, "EN")

# --------------------------------------------------------------- buck
box(700, 130, 130, 120, "U1", "LMR16006Y", [
    (700, 150, "VIN", "5", "L"), (700, 200, "EN", "4", "L"),
    (765, 250, "GND", "2", "B"), (800, 130, "FB", "3", "T"),
    (830, 150, "CB", "1", "R"), (830, 200, "SW", "6", "R")])
line([(680, 110), (680, 150), (700, 150)]); dot(680, 110)
line([(765, 250), (765, 262)]); gnd(765, 262)
line([(800, 130), (800, 80), (1230, 80)])
net(1000, 74, "FB")
line([(830, 150), (880, 150)])
cap(880, 150, 880, 200, "C3", "100n", side=1)
line([(830, 200), (960, 200)]); dot(880, 200)
net(860, 220, "SW")
dot(930, 200)
diode(930, 280, 930, 200, "D3", "PMEG6010", side=-1); gnd(930, 280)
coil(960, 200, 1060, "L1", "22u")
line([(1060, 200), (1400, 200)])
net(1265, 194, "BAT_LCD 3.75 V")
for x in (1110, 1160, 1230):
    dot(x, 200)
cap(1110, 200, 1110, 280, "C4", "22u", side=-1); gnd(1110, 280)
cap(1160, 200, 1160, 280, "C5", "22u"); gnd(1160, 280)
res(1230, 200, 1230, 80, "R2", "39k", side=-1)
dot(1230, 80)
line([(1230, 80), (1250, 80)])
res(1250, 80, 1340, 80, "R3", "10k")
line([(1340, 80), (1360, 80)]); gnd(1360, 80)
text(1190, 330, "0.765 V x (1 + 39k/10k) = 3.75 V", 12, "middle", "#555")

box(1400, 180, 80, 80, "J3", "pads", [(1400, 200, "+", "1", "L"), (1400, 240, "-", "2", "L")])
line([(1400, 240), (1380, 240)]); gnd(1380, 240)
text(1440, 285, "to display", 11, "middle", "#555")
text(1440, 299, "battery socket", 11, "middle", "#555")

# --------------------------------------------------------------- CAN
line([(110, 170), (170, 170), (170, 520), (600, 520)])
line([(110, 200), (150, 200), (150, 560), (600, 560)])
net(200, 514, "CANH"); net(200, 576, "CANL")
# ESD
box(225, 600, 70, 50, "D4", "NUP2105L", [])
line([(240, 520), (240, 600)]); dot(240, 520)
text(234, 596, "1", 10, "end", "#666")
line([(280, 560), (280, 600)]); dot(280, 560)
text(286, 596, "2", 10, "start", "#666")
line([(260, 650), (260, 662)]); gnd(260, 662)
text(266, 662, "3", 10, "start", "#666")
# split termination, not assembled
dot(380, 520)
res(380, 520, 380, 640, "R4", "60R (DNP)", dnp=True, side=-1)
line([(380, 640), (400, 640)], True, DNP)
res(400, 640, 480, 640, "R5", "60R (DNP)", dnp=True, side=1)
cap(380, 640, 380, 700, "C8", "4n7 (DNP)", dnp=True, side=-1); gnd(380, 700)
line([(480, 640), (500, 640)], True, DNP)
S.append('<rect x="490" y="585" width="20" height="30" fill="#fff" stroke="%s" stroke-width="2" stroke-dasharray="4 3"/>' % DNP)
line([(500, 640), (500, 615)], True, DNP)
line([(500, 585), (500, 560)], True, DNP); dot(500, 560)
text(516, 605, "JP1 (open)", 12, "start", DNP)

box(600, 480, 160, 170, "U2", "SN65HVD230", [
    (600, 520, "CANH", "7", "L"), (600, 560, "CANL", "6", "L"),
    (600, 610, "Vref", "5", "L"),
    (760, 500, "D", "1", "R"), (760, 540, "R", "4", "R"),
    (760, 580, "VCC", "3", "R"), (760, 625, "Rs", "8", "R"),
    (680, 650, "GND", "2", "B")])
line([(600, 610), (585, 610)])
line([(578, 603), (592, 617)]); line([(578, 617), (592, 603)])
text(572, 614, "nc", 11, "end", "#555")
line([(680, 650), (680, 662)]); gnd(680, 662)
line([(760, 625), (790, 625)]); gnd(790, 625)
text(700, 712, "Rs to GND: full speed, no slope control", 11, "start", "#555")

line([(760, 500), (1150, 500)]); net(800, 494, "TXD")
line([(760, 540), (940, 540)]); net(800, 534, "RXD_T")
res(940, 540, 1020, 540, "R6", "1k")
line([(1020, 540), (1150, 540)]); net(1040, 534, "RXD")
line([(760, 580), (1150, 580)]); net(950, 574, "+3V3 from display")
for x in (860, 920):
    dot(x, 580)
cap(860, 580, 860, 660, "C6", "100n", side=-1); gnd(860, 660)
cap(920, 580, 920, 660, "C7", "100n"); gnd(920, 660)

box(1150, 470, 90, 180, "J2", "pads", [
    (1150, 500, "IO44", "1", "L"), (1150, 540, "IO43", "2", "L"),
    (1150, 580, "3V3", "3", "L"), (1150, 620, "GND", "4", "L")])
line([(1150, 620), (1110, 620)]); gnd(1110, 620)
text(1195, 670, "to display", 11, "middle", "#555")
text(1195, 684, "UART socket 1..4", 11, "middle", "#555")
text(1260, 500, "GPIO44 = CAN TX", 12, "start", "#555")
text(1260, 540, "GPIO43 = CAN RX", 12, "start", "#555")

# ------------------------------------------------------------ check
WANT = {  # what this drawing connects, by ref.pin
    "J1": {"1": "VBAT", "2": "GND", "3": "CANH", "4": "CANL"},
    "F1": {"1": "VBAT", "2": "VBAT_F"}, "D1": {"1": "VIN", "2": "VBAT_F"},
    "D2": {"1": "VIN", "2": "GND"}, "C1": {"1": "VIN", "2": "GND"},
    "C2": {"1": "VIN", "2": "GND"}, "R1": {"1": "VIN", "2": "EN"},
    "U1": {"1": "CB", "2": "GND", "3": "FB", "4": "EN", "5": "VIN", "6": "SW"},
    "C3": {"1": "CB", "2": "SW"}, "D3": {"1": "SW", "2": "GND"},
    "L1": {"1": "SW", "2": "BAT_LCD"}, "R2": {"1": "BAT_LCD", "2": "FB"},
    "R3": {"1": "FB", "2": "GND"}, "C4": {"1": "BAT_LCD", "2": "GND"},
    "C5": {"1": "BAT_LCD", "2": "GND"}, "J3": {"1": "BAT_LCD", "2": "GND"},
    "U2": {"1": "TXD", "2": "GND", "3": "+3V3", "4": "RXD_T", "6": "CANL",
           "7": "CANH", "8": "GND"},
    "C6": {"1": "+3V3", "2": "GND"}, "C7": {"1": "+3V3", "2": "GND"},
    "D4": {"1": "CANH", "2": "CANL", "3": "GND"},
    "R4": {"1": "CANH", "2": "TMID"}, "R5": {"1": "TMID", "2": "TERM"},
    "C8": {"1": "TMID", "2": "GND"}, "JP1": {"1": "TERM", "2": "CANL"},
    "J2": {"1": "TXD", "2": "RXD", "3": "+3V3", "4": "GND"},
    "R6": {"1": "RXD", "2": "RXD_T"},
}
have = {r: p["pins"] for r, p in P.items() if not r.startswith("H")}
assert have == WANT, "drawing and design.py disagree"

with open(OUT, "w") as f:
    f.write('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1520 730" '
            'font-family="DejaVu Sans, Arial, sans-serif">\n'
            '<rect width="1520" height="730" fill="#fff"/>\n')
    f.write("\n".join(S))
    f.write("\n</svg>\n")
print("wrote", OUT)
