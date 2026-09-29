#!/usr/bin/env python3
"""Pre-render the round gauge faces into main/faces.c and main/faces.h.

Everything static on a page is baked into one RGB565 image: the radial
background, the bezel line, the groove the value arc runs in, the red zone,
ticks, scale numbers, the icon and the title. LVGL then only draws the value
arc (with its glow), the readout and the page dots on top, so the look can
have gradients and antialiased artwork without costing a frame.

The scale geometry is shared with ui_round.c through faces.h. The page table
(ranges, ticks, limits) lives here, since the faces are drawn from it, and is
written into faces.h for the UI.

Icons are drawn as vectors (unit box 0..100) at 8x and scaled down; they are
also written to preview/icons.png.

Run from the project root:  python tools/gen_faces.py [--size 480]
"""
import argparse
import math
import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
MAIN = os.path.join(ROOT, "main")
FONT_BOLD = os.path.join(ROOT, "assets", "fonts", "Orbitron-Bold.ttf")

SS = 4                      # supersampling of the faces

# scale geometry, shared with ui_round.c (as fractions of the radius R)
START, SWEEP = 135.0, 270.0  # degrees, 0 = 3 o'clock, clockwise
GROOVE_R = 0.912             # centre line of the groove / value arc
GROOVE_W = 0.092             # groove width
ARC_W = 0.062                # value arc width

# name, title, unit, lo, hi, minor step, major step, label div, warn, icon,
# note under the dial
PAGES = [
    ("WATER",   "WATER",   "°C", 40, 140, 5,   20,  1,    105,  "water",   None),
    ("OIL",     "OIL",     "°C", 40, 160, 5,   20,  1,    130,  "oil",     None),
    ("BOOST",   "BOOST",   "bar",     0,  2.5, 0.1, 0.5, 1,    2.2,  "turbo",   None),
    ("INTAKE",  "INTAKE",  "°C", 0,  100, 5,   20,  1,    60,   "intake",  None),
    ("EXHAUST", "EXHAUST", "°C", 0, 1000, 50,  200, 100,  750,  "exhaust", "x100"),
    ("RPM",     "ENGINE",  "rpm",     0, 5000, 250, 1000, 1000, 4500, "rpm",    "x1000"),
    ("DPF",     "DPF",     "%",       0,  100, 5,   20,  1,    80,   "dpf",     None),
]

C_BG_IN = (22, 25, 30)
C_BG_OUT = (0, 0, 0)
C_GROOVE = (12, 14, 17)
C_ZONE = (92, 14, 14)
C_ZONE_EDGE = (200, 36, 36)
C_MINOR = (92, 98, 106)
C_MAJOR = (225, 229, 234)
C_NUM = (170, 176, 184)
C_ICON = (235, 238, 242)
C_TITLE = (150, 156, 164)


# ------------------------------------------------------------------- icons
def _poly_line(d, pts, w, s, fill=255):
    d.line([(x * s, y * s) for x, y in pts], fill=fill, width=int(w * s),
           joint="curve")
    for x, y in (pts[0], pts[-1]):
        r = w * s / 2
        d.ellipse([x * s - r, y * s - r, x * s + r, y * s + r], fill=fill)


def _circle(d, cx, cy, r, s, fill=None, outline=None, w=0):
    d.ellipse([(cx - r) * s, (cy - r) * s, (cx + r) * s, (cy + r) * s],
              fill=fill, outline=outline, width=int(w * s))


def _rrect(d, x0, y0, x1, y1, rad, s, fill=None, outline=None, w=0):
    d.rounded_rectangle([x0 * s, y0 * s, x1 * s, y1 * s], radius=rad * s,
                        fill=fill, outline=outline, width=int(w * s))


def _wave(x0, x1, y, amp, n=2):
    pts = []
    for i in range(41):
        x = x0 + (x1 - x0) * i / 40
        pts.append((x, y + amp * math.sin(i / 40 * n * 2 * math.pi)))
    return pts


def _thermo(d, s, x, top, bot, r_bulb, w):
    """Thermometer: stem from top to the bulb centre at bot."""
    hw = w
    _rrect(d, x - hw, top, x + hw, bot, hw, s, outline=255, w=w * 0.75)
    _circle(d, x, bot, r_bulb, s, fill=255)
    d.line([(x * s, (top + hw) * s), (x * s, bot * s)], fill=255,
           width=int(w * 0.8 * s))


def icon_water(d, s):
    _thermo(d, s, 50, 6, 60, 11, 7)
    for y in (18, 30, 42):
        _poly_line(d, [(62, y), (72, y)], 5, s)
    _poly_line(d, _wave(8, 92, 80, 4), 6, s)
    _poly_line(d, _wave(8, 92, 94, 4), 6, s)


def icon_oil(d, s):
    # can body, cap, handle, spout, drop
    body = [(24, 52), (66, 52), (80, 80), (24, 80)]
    d.polygon([(x * s, y * s) for x, y in body], fill=255)
    _rrect(d, 36, 42, 50, 52, 2, s, fill=255)
    _rrect(d, 32, 38, 54, 43, 2, s, fill=255)
    _poly_line(d, [(24, 58), (10, 50), (6, 60), (24, 72)], 6, s)
    _poly_line(d, [(64, 58), (96, 44)], 7, s)
    # drop under the spout
    d.polygon([(93 * s, 56 * s), (88 * s, 68 * s), (98 * s, 68 * s)],
              fill=255)
    _circle(d, 93, 70, 5, s, fill=255)


def icon_turbo(d, s):
    cx, cy = 44, 56
    # volute: a spiral-ish housing, opening into the outlet at the top
    _circle(d, cx, cy, 34, s, outline=255, w=8)
    _rrect(d, cx, 14, 96, 34, 3, s, fill=255)
    _rrect(d, cx + 4, 20, 90, 28, 2, s, fill=0)
    # compressor wheel
    _circle(d, cx, cy, 8, s, fill=255)
    for k in range(7):
        a = k * 2 * math.pi / 7
        pts = []
        for i in range(9):
            t = i / 8
            r = 10 + 13 * t
            ang = a + 0.9 * t
            pts.append((cx + r * math.cos(ang), cy + r * math.sin(ang)))
        _poly_line(d, pts, 4.5, s)


def icon_intake(d, s):
    _thermo(d, s, 74, 8, 72, 12, 7)
    _poly_line(d, [(6, 36), (44, 36)], 6, s)
    _poly_line(d, [(44, 36), (52, 32), (52, 24), (44, 22), (38, 26)], 6, s)
    _poly_line(d, [(14, 52), (54, 52)], 6, s)
    _poly_line(d, [(6, 68), (44, 68)], 6, s)
    _poly_line(d, [(44, 68), (52, 72), (52, 80), (44, 82), (38, 78)], 6, s)


def icon_exhaust(d, s):
    # tailpipe with a flared tip, heat rising off its end
    _rrect(d, 2, 64, 50, 82, 4, s, fill=255)
    d.polygon([(48 * s, 61 * s), (62 * s, 57 * s), (62 * s, 89 * s),
               (48 * s, 85 * s)], fill=255)
    for x in (68, 81, 94):
        pts = [(x + 5 * math.sin(i / 20 * 2 * math.pi), 50 - 44 * i / 20)
               for i in range(21)]
        _poly_line(d, pts, 5.5, s)


def icon_rpm(d, s):
    cx, cy, r = 50, 58, 40
    d.arc([(cx - r) * s, (cy - r) * s, (cx + r) * s, (cy + r) * s],
          start=150, end=390, fill=255, width=int(7 * s))
    for k in range(7):
        a = math.radians(150 + k * 40)
        _poly_line(d, [(cx + (r - 8) * math.cos(a), cy + (r - 8) * math.sin(a)),
                       (cx + (r - 16) * math.cos(a), cy + (r - 16) * math.sin(a))],
                   5, s)
    a = math.radians(300)
    _poly_line(d, [(cx, cy), (cx + 30 * math.cos(a), cy + 30 * math.sin(a))],
               6, s)
    _circle(d, cx, cy, 8, s, fill=255)


def icon_dpf(d, s):
    _rrect(d, 18, 26, 82, 82, 10, s, outline=255, w=7)
    _rrect(d, 2, 46, 20, 62, 3, s, fill=255)
    _rrect(d, 80, 46, 98, 62, 3, s, fill=255)
    for i in range(4):
        for j in range(3):
            _circle(d, 32 + i * 12, 40 + j * 14, 3.6, s, fill=255)


ICONS = {
    "water": icon_water, "oil": icon_oil, "turbo": icon_turbo,
    "intake": icon_intake, "exhaust": icon_exhaust, "rpm": icon_rpm,
    "dpf": icon_dpf,
}


def icon_mask(name, px):
    """Antialiased alpha mask of an icon, px square."""
    s = 8 * px / 100.0
    big = Image.new("L", (int(100 * s), int(100 * s)), 0)
    ICONS[name](ImageDraw.Draw(big), s)
    return big.resize((px, px), Image.LANCZOS)


# ------------------------------------------------------------------- faces
def value_angle(p, v):
    lo, hi = p[3], p[4]
    return START + SWEEP * (v - lo) / (hi - lo)


def fmt_label(v, div):
    x = v / div
    if abs(x - round(x)) < 1e-6:
        return "%d" % round(x)
    return ("%.1f" % x).rstrip("0")


def draw_face(p, size):
    S = size * SS
    c = S / 2
    R = S / 2
    img = Image.new("RGB", (S, S), C_BG_OUT)
    d = ImageDraw.Draw(img)

    # radial background: lighter in the middle
    steps = 48
    for i in range(steps):
        t = i / (steps - 1)
        r = R * (1 - t)
        k = t ** 1.6
        col = tuple(int(C_BG_OUT[j] + (C_BG_IN[j] - C_BG_OUT[j]) * k)
                    for j in range(3))
        d.ellipse([c - r, c - r, c + r, c + r], fill=col)

    # bezel line just inside the edge: lit from above
    for i in range(360):
        a0, a1 = i, i + 1.5
        lum = 0.5 + 0.5 * math.sin(math.radians(-(i + 0.5)))
        col = tuple(int(30 + 60 * lum) for _ in range(3))
        d.arc([c - R + 2 * SS, c - R + 2 * SS, c + R - 2 * SS, c + R - 2 * SS],
              a0, a1, fill=col, width=int(2 * SS))

    # groove the value arc runs in
    gr, gw = GROOVE_R * R, GROOVE_W * R
    box = [c - gr - gw / 2, c - gr - gw / 2, c + gr + gw / 2, c + gr + gw / 2]
    d.arc(box, START, START + SWEEP, fill=C_GROOVE, width=int(gw))
    # red zone: tinted groove and a thin bright rim on its outside
    a_w = value_angle(p, p[8])
    d.arc(box, a_w, START + SWEEP, fill=C_ZONE, width=int(gw))
    d.arc(box, a_w, START + SWEEP, fill=C_ZONE_EDGE, width=int(1.2 * SS))
    # rounded groove ends
    for a in (START, START + SWEEP):
        x = c + gr * math.cos(math.radians(a))
        y = c + gr * math.sin(math.radians(a))
        col = C_ZONE if a == START + SWEEP else C_GROOVE
        d.ellipse([x - gw / 2, y - gw / 2, x + gw / 2, y + gw / 2], fill=col)

    # ticks and numbers
    lo, hi, minor, major, div = p[3], p[4], p[5], p[6], p[7]
    font = ImageFont.truetype(FONT_BOLD, int(0.108 * R))
    n = int(round((hi - lo) / minor))
    per = int(round(major / minor))
    r_out = (GROOVE_R - GROOVE_W / 2 - 0.018) * R
    for k in range(n + 1):
        v = lo + k * minor
        a = math.radians(value_angle(p, v))
        is_major = k % per == 0
        ln = (0.075 if is_major else 0.035) * R
        w = (0.017 if is_major else 0.009) * R
        col = C_MAJOR if is_major else C_MINOR
        if v >= p[8] - 1e-9:
            col = (230, 70, 70) if is_major else (130, 40, 40)
        d.line([(c + r_out * math.cos(a), c + r_out * math.sin(a)),
                (c + (r_out - ln) * math.cos(a), c + (r_out - ln) * math.sin(a))],
               fill=col, width=int(w))
        if is_major:
            txt = fmt_label(v, div)
            rt = r_out - ln - 0.105 * R
            x = c + rt * math.cos(a)
            y = c + rt * math.sin(a)
            d.text((x, y), txt, font=font, fill=C_NUM, anchor="mm")

    # icon and title above the readout
    ipx = int(0.23 * R)
    m = icon_mask(p[9], ipx)
    ink = Image.new("RGB", m.size, C_ICON)
    img.paste(ink, (int(c - ipx / 2), int(c - 0.53 * R)), m)
    tf = ImageFont.truetype(FONT_BOLD, int(0.075 * R))
    _spaced(d, (c, c - 0.26 * R), p[1], tf, C_TITLE, 0.02 * R)
    if p[10]:
        nf = ImageFont.truetype(FONT_BOLD, int(0.07 * R))
        d.text((c, c + 0.60 * R), p[10], font=nf, fill=C_MINOR, anchor="mm")

    img = img.resize((size, size), Image.LANCZOS)
    # outside the circle: black, the panel does not show it anyway
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).ellipse([0, 0, size - 1, size - 1], fill=255)
    out = Image.new("RGB", (size, size), (0, 0, 0))
    out.paste(img, (0, 0), mask)
    return out


def _spaced(d, centre, text, font, fill, gap):
    widths = [d.textlength(ch, font=font) for ch in text]
    total = sum(widths) + gap * (len(text) - 1)
    x = centre[0] - total / 2
    for ch, w in zip(text, widths):
        d.text((x, centre[1]), ch, font=font, fill=fill, anchor="lm")
        x += w + gap


# ------------------------------------------------------------------- C out
def rgb565(img):
    px = img.load()
    w, h = img.size
    out = []
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            out.append(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", type=int, default=480)
    size = ap.parse_args().size
    R = size / 2

    lines = ["/* Generated by tools/gen_faces.py, do not edit. */",
             '#include "faces.h"', ""]
    names = []
    prev = os.path.join(ROOT, "preview", "faces")
    os.makedirs(prev, exist_ok=True)
    for p in PAGES:
        face = draw_face(p, size)
        face.save(os.path.join(prev, p[0].lower() + ".png"))
        name = "face_" + p[0].lower()
        names.append(name)
        data = rgb565(face)
        lines.append("static const uint16_t %s_px[%d] = {" % (name, len(data)))
        for i in range(0, len(data), 16):
            lines.append("    " + ", ".join("0x%04X" % v for v in data[i:i + 16]) + ",")
        lines.append("};")
        lines.append("const lv_img_dsc_t %s = {" % name)
        lines.append("    .header.cf = LV_IMG_CF_TRUE_COLOR, .header.w = %d, "
                     ".header.h = %d," % (size, size))
        lines.append("    .data_size = sizeof %s_px, .data = (const uint8_t *)%s_px,"
                     % (name, name))
        lines.append("};")
        lines.append("")
    lines.append("const lv_img_dsc_t *const face_img[FACE_COUNT] = {")
    lines.append("    " + ", ".join("&" + n for n in names) + ",")
    lines.append("};")
    open(os.path.join(MAIN, "faces.c"), "w").write("\n".join(lines) + "\n")

    def num(v):
        s = "%g" % v
        return (s if "." in s else s + ".0") + "f"

    h = ["/* Generated by tools/gen_faces.py, do not edit. */",
         "#pragma once",
         '#include "lvgl.h"',
         "",
         "#define FACE_SIZE     %d" % size,
         "#define FACE_START    %d     /* degrees, 0 = 3 o'clock, clockwise */" % START,
         "#define FACE_SWEEP    %d" % SWEEP,
         "#define FACE_ARC_R    %d     /* centre line of the value arc */" % round(GROOVE_R * R),
         "#define FACE_ARC_W    %d" % round(ARC_W * R),
         "#define FACE_COUNT    %d" % len(PAGES),
         "",
         "/* one row per page, in page order */",
         "typedef struct {",
         "    const char *unit;",
         "    float lo, hi, warn;",
         "} face_page_t;",
         "",
         "static const face_page_t FACE_PAGE[FACE_COUNT] = {"]
    for p in PAGES:
        unit = p[2].replace("°", "\\xC2\\xB0\" \"")
        h.append('    { "%s", %s, %s, %s },   /* %s */'
                 % (unit, num(float(p[3])), num(float(p[4])), num(float(p[8])), p[0]))
    h += ["};", "",
          "extern const lv_img_dsc_t *const face_img[FACE_COUNT];", ""]
    open(os.path.join(MAIN, "faces.h"), "w").write("\n".join(h))

    # icon sheet for the docs
    px = 96
    sheet = Image.new("RGB", (px * len(ICONS) + 16 * (len(ICONS) + 1), px + 32),
                      (0, 0, 0))
    for i, name in enumerate(ICONS):
        m = icon_mask(name, px)
        sheet.paste(Image.new("RGB", m.size, C_ICON), (16 + i * (px + 16), 16), m)
    sheet.save(os.path.join(ROOT, "preview", "icons.png"))
    print("wrote main/faces.c, main/faces.h, preview/faces/*.png, preview/icons.png")


if __name__ == "__main__":
    main()
