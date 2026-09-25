#!/usr/bin/env python3
"""Pixel-accurate preview of the settings screen.

Mirrors the geometry in main/ui_menu.c and the defaults in main/settings.c so
the menu can be judged without flashing. Pass a row index to scroll to it.
"""
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont

W, H = 800, 480
DJ = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"

Y = (0xF5, 0xC5, 0x18)
WHT = (0xFF, 0xFF, 0xFF)
GREY = (0x8A, 0x90, 0x96)
LINE = (0x3A, 0x3D, 0x42)
BTN = (0x1B, 0x1D, 0x20)
CARDBG = (0x0D, 0x0E, 0x10)
RED = (0xE2, 0x24, 0x24)
GREEN = (0x25, 0xC2, 0x5A)

# ---- geometry, keep in sync with ui_menu.c --------------------------------
LIST_X, LIST_Y, LIST_W, LIST_H = 8, 44, 784, 366
ROW_W, ROW_H, ROW_GAP = 760, 56, 6
LBL_X = 16
VAL_X, VAL_W = 330, 190
BTN_W, BTN_H = 64, 44
BTN1_R, BTN2_R = 84, 12
FOOT_Y = 418

f18 = ImageFont.truetype(DJ, 20)
f13 = ImageFont.truetype(DJ, 14)

# ---- rows, mirroring ui_menu.c and settings.c defaults --------------------
ROWS = [
    ("Shift flash",       "ON"),
    ("Shift flash at",    "7000 rpm"),
    ("Shift flash level", "80 %"),
    ("Water temp",      "105 \u00b0C"),
    ("Oil temp",        "130 \u00b0C"),
    ("Intake air temp", "60 \u00b0C"),
    ("Oil press min",   "1.00 bar"),
    ("Fuel press min",  "2.50 bar"),
    ("Boost limit",     "1.20 bar"),
    ("AFR lean limit",  "16.0"),
    ("Brightness",      "100 %"),
    ("Rev counter max", "8000 rpm"),
    ("Redline",         "7000 rpm"),
    ("Fuel",            "Petrol"),
    ("Baro offset",     "1.00 bar"),
    ("Demo mode",       "OFF"),
]
TOGGLE = {0, 13, 15}          # one SET button instead of minus / plus

scroll = int(sys.argv[1]) * (ROW_H + ROW_GAP) if len(sys.argv) > 1 else 0

img = Image.new("RGB", (W, H), (0, 0, 0))
d = ImageDraw.Draw(img)


def btn(x, y, w, h, txt, border=LINE):
    d.rounded_rectangle([x, y, x + w, y + h], radius=6, fill=BTN,
                        outline=border, width=1)
    tw = d.textlength(txt, font=f18)
    d.text((x + (w - tw) / 2, y + h / 2 - 13), txt, font=f18, fill=WHT)


d.text((16, 12), "SETTINGS", font=f18, fill=Y)
d.text((140, 16), "flash is rev limit only \u00b7 swipe to scroll",
       font=f13, fill=GREY)

for i, (label, value) in enumerate(ROWS):
    ry = LIST_Y + i * (ROW_H + ROW_GAP) - scroll
    if ry + ROW_H < LIST_Y or ry > LIST_Y + LIST_H:
        continue
    top = max(ry, LIST_Y)
    bot = min(ry + ROW_H, LIST_Y + LIST_H)
    if bot - top < 4:
        continue

    d.rounded_rectangle([LIST_X, ry, LIST_X + ROW_W, ry + ROW_H], radius=7,
                        fill=CARDBG, outline=LINE, width=1)

    d.text((LIST_X + LBL_X, ry + ROW_H / 2 - 13), label, font=f18, fill=WHT)

    tw = d.textlength(value, font=f18)
    d.text((LIST_X + VAL_X + VAL_W - tw, ry + ROW_H / 2 - 13), value,
           font=f18, fill=Y)

    by = ry + (ROW_H - BTN_H) // 2
    if i in TOGGLE:
        btn(LIST_X + ROW_W - BTN2_R - BTN_W, by, BTN_W, BTN_H, "SET")
    else:
        btn(LIST_X + ROW_W - BTN1_R - BTN_W, by, BTN_W, BTN_H, "-")
        btn(LIST_X + ROW_W - BTN2_R - BTN_W, by, BTN_W, BTN_H, "+")

# clip whatever spilled outside the list viewport, as LVGL would
d.rectangle([0, 0, W, LIST_Y - 1], fill=(0, 0, 0))
d.rectangle([0, LIST_Y + LIST_H + 1, W, H], fill=(0, 0, 0))
d.text((16, 12), "SETTINGS", font=f18, fill=Y)
d.text((140, 16), "flash is rev limit only \u00b7 swipe to scroll",
       font=f13, fill=GREY)

btn(8, FOOT_Y, 220, 52, "SAVE & CLOSE", GREEN)
btn(240, FOOT_Y, 170, 52, "DEFAULTS", RED)

SUBTLE = (0x50, 0x53, 0x5A)
v1 = "NOT STOCK  v1.0"
v2 = "built Aug  2 2026 09:41   LVGL 8.4.0   IDF v5.5"
d.text((452 + 340 - d.textlength(v1, font=f13), FOOT_Y + 6), v1,
       font=f13, fill=GREY)
d.text((452 + 340 - d.textlength(v2, font=f13), FOOT_Y + 26), v2,
       font=f13, fill=SUBTLE)

# scrollbar, LVGL draws one on the right when the content overflows
total = len(ROWS) * (ROW_H + ROW_GAP)
bar_h = int(LIST_H * LIST_H / total)
bar_y = LIST_Y + int(LIST_H * scroll / total)
d.rounded_rectangle([LIST_X + LIST_W - 4, bar_y,
                     LIST_X + LIST_W - 1, bar_y + bar_h],
                    radius=2, fill=(0x50, 0x53, 0x57))

out = sys.argv[2] if len(sys.argv) > 2 else \
    "/mnt/user-data/outputs/notstock-menu-preview.png"
img.save(out)
print("wrote", out)
