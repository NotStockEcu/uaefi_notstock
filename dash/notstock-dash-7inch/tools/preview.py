#!/usr/bin/env python3
"""Pixel-accurate preview of the 800x480 LVGL layout.

Mirrors the LY_* constants and lv_meter geometry from main/ui.c so the layout
can be judged without flashing. Not part of the firmware.
"""
import math
import os
import re
import sys
from PIL import Image, ImageDraw, ImageFont

W, H = 800, 480
DJ = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"

Y = (0xF5, 0xC5, 0x18)
Y_DIM = (0x7A, 0x63, 0x0E)
RED = (0xE2, 0x24, 0x24)
REDDIM = (0x3D, 0x0D, 0x0D)
WHT = (0xFF, 0xFF, 0xFF)
LBL = (0xC8, 0xCB, 0xCE)
GREY = (0x8A, 0x90, 0x96)
DIM = (0x24, 0x26, 0x29)
BAND_DIM = (0x33, 0x36, 0x3A)
LINE = (0x3A, 0x3D, 0x42)
HUB = (0x0A, 0x0A, 0x0A)
HUBED = (0x44, 0x44, 0x44)
TICKC = (0xEC, 0xEC, 0xEC)
MINC = (0x6E, 0x72, 0x76)
TICKLBL = (0xA8, 0xAD, 0xB2)
GREEN = (0x25, 0xC2, 0x5A)
OFFC = (0x40, 0x43, 0x47)

# ---- geometry, keep in sync with ui.c -------------------------------------
M = 6                          # outer margin

GAUGE_W = 250
GX_L, GX_R = M, W - M - GAUGE_W
TITLE_Y = 4
METER, METER_Y = 202, 26
VAL_Y = 222                    # big readout, below the dial
SUB_Y = 274

MID_X = GX_L + GAUGE_W + 10
MID_W = GX_R - MID_X - 10
RPM_SCALE_Y = 36
RPM_BAR_Y, RPM_BAR_H = 54, 30
RPM_NUM_Y = 92
RULE_Y = 178
SPEED_LBL_Y = 188
SPEED_ROW_Y = 214

CARD_Y, CARD_H = 302, 104
CARD_STEP = (W - 2 * M + 8) // 5
CARD_W = CARD_STEP - 8

BAR_Y, BAR_H = 418, 46

# live fill arc, must match LY_BAND_W / LY_BAND_MOD in ui.c
BAND_W = 7
BAND_MOD = -3

f_rpm = ImageFont.truetype(DJ, 72)
f_val = ImageFont.truetype(DJ, 48)
f_card = ImageFont.truetype(DJ, 34)
f_tick = ImageFont.truetype(DJ, 16)
f_h = ImageFont.truetype(DJ, 20)
f_s = ImageFont.truetype(DJ, 14)

img = Image.new("RGB", (W, H), (0, 0, 0))
d = ImageDraw.Draw(img)


def ctr(txt, font, x, y, w, col):
    d.text((x + (w - d.textlength(txt, font=font)) / 2, y), txt,
           font=font, fill=col)


def pol(cx, cy, r, deg):
    a = math.radians(deg)
    return cx + r * math.cos(a), cy + r * math.sin(a)


# ---------------------------------------------------------------- icon load
icons = {}
here = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(here, "..", "main", "icons.c")).read()
for m in re.finditer(
        r'static const uint8_t (\w+)_map\[\d+\] = \{(.*?)\};\s*'
        r'const lv_img_dsc_t \w+ = \{.*?\.w = (\d+), \.h = (\d+)',
        src, re.S):
    vals = [int(x, 16) for x in re.findall(r'0x([0-9a-f]{2})', m.group(2))]
    w, h = int(m.group(3)), int(m.group(4))
    ic = Image.new("L", (w, h))
    ic.putdata(vals[:w * h])
    icons[m.group(1)] = ic


def paste_icon(name, cx, cy, col):
    ic = icons[name]
    layer = Image.new("RGB", ic.size, col)
    img.paste(layer, (int(cx - ic.width / 2), int(cy - ic.height / 2)), ic)


# -------------------------------------------------------------------- gauge
# The dial faces, needles and hub are the exact PNGs that tools/gen_dials.py
# bakes into main/dials.c, so this preview is what the panel actually shows.
ART = os.path.join(here, "..", "build_art")
_art = {}
for n in ("dial_boost", "dial_afr", "needle_y", "needle_r", "hub", "logo"):
    _art[n] = Image.open(os.path.join(ART, n + ".png")).convert("RGBA")

NEEDLE_PIVOT = (18, 13)
SWEEP_START, SWEEP = 145, 250


def gauge(bx, title, face, unit, vmin, vmax, val, val_dec,
          fill_from=None, sub=None, warn=False):
    ctr(title, f_h, bx, TITLE_Y, GAUGE_W, Y)
    mx = bx + (GAUGE_W - METER) // 2
    cx, cy = mx + METER // 2, METER_Y + METER // 2
    img.paste(_art[face], (mx, METER_Y), _art[face])

    ang = SWEEP_START + (val - vmin) / (vmax - vmin) * SWEEP

    if fill_from is not None:
        a0 = SWEEP_START + (fill_from - vmin) / (vmax - vmin) * SWEEP
        lo, hi = sorted((a0, ang))
        r_out = METER // 2 + BAND_MOD
        d.arc([cx - r_out + BAND_W // 2, cy - r_out + BAND_W // 2,
               cx + r_out - BAND_W // 2, cy + r_out - BAND_W // 2],
              lo, hi, fill=RED if warn else Y, width=BAND_W)

    nd = _art["needle_r" if warn else "needle_y"]
    big = Image.new("RGBA", (METER * 2, METER * 2), (0, 0, 0, 0))
    big.paste(nd, (METER - NEEDLE_PIVOT[0], METER - NEEDLE_PIVOT[1]), nd)
    big = big.rotate(-ang, resample=Image.BICUBIC, center=(METER, METER))
    img.paste(big, (cx - METER, cy - METER), big)

    hub = _art["hub"]
    img.paste(hub, (cx - hub.width // 2, cy - hub.height // 2), hub)

    txt = f"{val:.2f}" if val_dec == 2 else f"{val:.1f}"
    tw = d.textlength(txt, font=f_val)
    uw = d.textlength(unit, font=f_s) if unit else 0
    x0 = bx + (GAUGE_W - (tw + (6 + uw if unit else 0))) / 2
    d.text((x0, VAL_Y), txt, font=f_val, fill=RED if warn else WHT)
    if unit:
        d.text((x0 + tw + 6, VAL_Y + 32), unit, font=f_s, fill=GREY)
    if sub:
        ctr(sub, f_s, bx, SUB_Y, GAUGE_W, Y_DIM)


V = dict(boost=1.42, afr=11.8, rpm=4860, speed=112, clt=92, oilt=96,
         oilp=4.8, iat=34, fuelp=3.5, fan=True, als=False, link="CAN", alarm=False)

gauge(GX_L, "BOOST", "dial_boost", "bar", -1, 2, V["boost"], 2,
      fill_from=0, warn=V["boost"] >= 1.2)
gauge(GX_R, "AFR", "dial_afr", "", 10, 18, V["afr"], 1,
      sub="\u03bb %.2f" % (V["afr"] / 14.7))

# ------------------------------------------------------------------- middle
ctr("RPM", f_h, MID_X, TITLE_Y, MID_W - 48, Y)
d.text((MID_X + MID_W - 46, TITLE_Y + 6), "x1000", font=f_s, fill=Y_DIM)

RPM_SEGS, RPM_PITCH, RPM_SEG_W = 33, 8, 6
RPM_X0, RPM_LBL_STEP = MID_X + 2, 33

for i in range(9):
    ctr(str(i), f_tick, RPM_X0 + i * RPM_LBL_STEP - 10, RPM_SCALE_Y, 20, GREY)

for i in range(RPM_SEGS):
    rl = (i + 1) * 8000 / RPM_SEGS > 7000
    on = i < round(V["rpm"] / 8000 * RPM_SEGS)
    col = (RED if rl else Y) if on else (REDDIM if rl else DIM)
    x = RPM_X0 + i * RPM_PITCH
    d.rectangle([x, RPM_BAR_Y, x + RPM_SEG_W - 1, RPM_BAR_Y + RPM_BAR_H],
                fill=col)

ctr(str(V["rpm"]), f_rpm, MID_X, RPM_NUM_Y, MID_W, WHT)
d.rectangle([MID_X + 40, RULE_Y, MID_X + MID_W - 40, RULE_Y], fill=LINE)
ctr("SPEED", f_h, MID_X, SPEED_LBL_Y, MID_W, Y)

sp = str(V["speed"])
w1 = d.textlength(sp, font=f_val)
w2 = d.textlength("km/h", font=f_s)
x0 = MID_X + (MID_W - (w1 + 7 + w2)) / 2
d.text((x0, SPEED_ROW_Y), sp, font=f_val, fill=WHT)
d.text((x0 + w1 + 7, SPEED_ROW_Y + 32), "km/h", font=f_s, fill=GREY)

# -------------------------------------------------------------------- cards
CARDS = [("WATER", "\u00b0C", "ic_water", 0, 120, 0, V["clt"], False),
         ("OIL TEMP", "\u00b0C", "ic_oiltemp", 0, 150, 0, V["oilt"], False),
         ("OIL PRESS", "bar", "ic_oilpress", 0, 8, 1, V["oilp"], False),
         ("IAT", "\u00b0C", "ic_iat", 0, 80, 0, V["iat"], False),
         ("FUEL PRESS", "bar", "ic_fuel", 0, 6, 1, V["fuelp"], False)]

for i, (lab, unit, icon, lo, hi, dec, val, warn) in enumerate(CARDS):
    x = M + i * CARD_STEP
    col = RED if warn else Y
    d.rounded_rectangle([x, CARD_Y, x + CARD_W, CARD_Y + CARD_H], radius=7,
                        fill=(0x0D, 0x0E, 0x10),
                        outline=RED if warn else LINE, width=1)
    paste_icon(icon, x + 25, CARD_Y + 23, col)
    d.text((x + 51, CARD_Y + 14), lab, font=f_s, fill=LBL)

    # continuous bar rather than segments: cleaner at this size
    bx0, bx1 = x + 12, x + CARD_W - 12
    d.rounded_rectangle([bx0, CARD_Y + 40, bx1, CARD_Y + 45], radius=2,
                        fill=DIM)
    f = min(1, max(0, (val - lo) / (hi - lo)))
    if f > 0:
        d.rounded_rectangle([bx0, CARD_Y + 40, bx0 + (bx1 - bx0) * f,
                             CARD_Y + 45], radius=2, fill=col)

    vt = f"{val:.1f}" if dec == 1 else f"{val:.0f}"
    d.text((x + 12, CARD_Y + 54), vt, font=f_card, fill=RED if warn else WHT)
    vw = d.textlength(vt, font=f_card)
    d.text((x + 12 + vw + 6, CARD_Y + 74), unit, font=f_s, fill=GREY)

# --------------------------------------------------------------- bottom bar
d.rounded_rectangle([M, BAR_Y, W - M, BAR_Y + BAR_H], radius=7,
                    outline=LINE, width=1)


def flag(cx, name, icon, on):
    paste_icon(icon, cx, BAR_Y + BAR_H // 2, Y if on else OFFC)
    d.text((cx + 20, BAR_Y + 8), name, font=f_s, fill=GREY)
    d.text((cx + 20, BAR_Y + 23), "ON" if on else "OFF", font=f_s,
           fill=Y if on else OFFC)


flag(M + 30, "FAN", "ic_fan", V["fan"])
flag(M + 122, "ALS", "ic_flame", V["als"])
d.rectangle([M + 92, BAR_Y + 10, M + 92, BAR_Y + BAR_H - 10], fill=LINE)
d.rectangle([M + 190, BAR_Y + 10, M + 190, BAR_Y + BAR_H - 10], fill=LINE)

logo = _art["logo"]
img.paste(logo, ((W - logo.width) // 2, BAR_Y + 3), logo)
sub = "N O T   S T A B L E"
d.text(((W - d.textlength(sub, font=f_s)) / 2, BAR_Y + 28), sub, font=f_s,
       fill=(0x50, 0x53, 0x57))

d.rectangle([W - 132, BAR_Y + 10, W - 132, BAR_Y + BAR_H - 10], fill=LINE)
lt = V["link"]
lc = GREEN if lt == "CAN" else (Y if lt == "DEMO" else RED)
d.ellipse([W - 116, BAR_Y + BAR_H // 2 - 4, W - 108, BAR_Y + BAR_H // 2 + 4],
          fill=lc)
d.text((W - 98, BAR_Y + BAR_H // 2 - 8), lt, font=f_s,
       fill=GREY if lt == "CAN" else lc)

if V.get("alarm"):
    wash = Image.new("RGBA", (W, H), RED + (int(255 * 0.45),))
    img.paste(Image.alpha_composite(img.convert("RGBA"), wash).convert("RGB"),
              (0, 0))

img.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "notstock-dash-800x480-preview.png"))
print("preview written")
