#!/usr/bin/env python3
"""Pre-render the static gauge artwork into LVGL C arrays.

The scales never change, so there is no reason to rebuild them out of LVGL
primitives every frame. Rendering them here with 4x supersampling buys
hairline ticks, antialiased bands and properly hinted labels, none of which
LVGL 8 draws well on its own. The firmware then only rotates a needle and
rewrites a number.

Every image has its pivot in its centre and may be any size; ui.c places
it by that centre.

Style follows a classic analogue cluster: black background, no dial face,
white ticks and numbers, red needles, grey metal hub caps, red and yellow
zones.

Outputs
    main/dials.c   RGB565 scale images, RGBA needles and hubs
    main/dials.h   every geometry number ui.c needs to line up with the art:
                   image sizes, where the pivot sits inside each image, sweep
                   angles and needle pivots. ui.c never repeats these by hand.
    build_art/*.png  the same images, for eyeballing

Run from the project root:  python tools/gen_dials.py
"""
import math
import os

from PIL import Image, ImageDraw, ImageFont

SS = 4                       # supersample factor

FONT = os.environ.get(
    "DASH_FONT", "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf")

WHITE = (0xF2, 0xF2, 0xF2)
WHITE_DIM = (0xB8, 0xBC, 0xC0)
LINE = (0xE6, 0xE6, 0xE6)
RED = (0xE2, 0x24, 0x24)
YELLOW = (0xF5, 0xC5, 0x18)
NEEDLE = (0xE8, 0x1E, 0x10)
NEEDLE_EDGE = (0x70, 0x0C, 0x06)

# ------------------------------------------------------------------ big dials
# Speedometer and rev counter. Numbers sit outside the ticks, the bottom 90
# degrees are open for the unit label.
BIG = 280                    # image is BIG x BIG, pivot in the centre
BIG_SWEEP_START = 135        # LVGL angle: 0 = 3 o'clock, clockwise
BIG_SWEEP = 270
BIG_R_TICK = 93              # outer end of the ticks
BIG_TICK_MAJ, BIG_TICK_MIN = 16, 8
BIG_LABEL_GAP = 5            # clear space between tick ends and numbers
BIG_LABEL_PX = 21
BIG_NEEDLE_LEN = 88          # pivot to tip
BIG_HUB = 42

SPEED_MAX = 240              # km/h
SPEED_MAJOR = 20
SPEED_MINOR = 10

RPM_MAX = 8000
RPM_REDLINE = 7000
RPM_MAJOR = 1000
RPM_MINOR = 250

# ---------------------------------------------------------------- temp gauges
# Water and intake air: a short arc across the top, needle swinging up from a
# hub below it, red zone above the hot end. Same drawing for both.
TEMP_W, TEMP_H = 120, 156    # pivot in the centre
TEMP_SWEEP_START = 220
TEMP_SWEEP = 100
TEMP_R = 60                  # the arc line
TEMP_NEEDLE_LEN = 57
TEMP_HUB = 28

CLT_MIN, CLT_MAX, CLT_RED = 40, 130, 105
IAT_MIN, IAT_MAX, IAT_RED = 0, 80, 60

# --------------------------------------------------------------- turbo gauges
# Boost and AFR: a long sweep from 9 o'clock over the top to just past
# 3 o'clock, thin line with thick coloured zones on the outside.
TURBO_W, TURBO_H = 120, 124
TURBO_SWEEP_START = 180
TURBO_SWEEP = 210
TURBO_R = 43                 # the thin line
TURBO_ZONE_W = 11            # zones stick out this far past the line
TURBO_NEEDLE_LEN = 41
TURBO_HUB = 28

BOOST_MIN, BOOST_MAX = -1.0, 2.0
BOOST_ZONES = [(0.8, 1.2, YELLOW), (1.2, 2.0, RED)]
BOOST_MARK = 0.0             # small tick at atmospheric

AFR_MIN, AFR_MAX = 10.0, 18.0
AFR_ZONES = [(10.0, 11.0, YELLOW), (16.0, 18.0, RED)]
AFR_MARK = 14.7


def u(v):
    return v * SS


def font(px):
    return ImageFont.truetype(FONT, int(round(px * SS)))


def canvas(w, h, mode="RGB"):
    fill = (0, 0, 0) if mode == "RGB" else (0, 0, 0, 0)
    img = Image.new(mode, (w * SS, h * SS), fill)
    return img, ImageDraw.Draw(img)


def pt(cx, cy, r, a_deg):
    a = math.radians(a_deg)
    return cx + r * math.cos(a), cy + r * math.sin(a)


def arc_poly(d, cx, cy, r_in, r_out, a0, a1, col):
    """Filled annular sector. PIL's arc() has no antialiasing and a single
    width, so bands get drawn as polygons at supersampled resolution."""
    if a1 < a0:
        a0, a1 = a1, a0
    steps = max(8, int((a1 - a0) * 3))
    pts = [pt(cx, cy, r_out, a0 + (a1 - a0) * i / steps)
           for i in range(steps + 1)]
    pts += [pt(cx, cy, r_in, a0 + (a1 - a0) * i / steps)
            for i in range(steps, -1, -1)]
    d.polygon(pts, fill=col)


def radial_line(d, cx, cy, r0, r1, a_deg, w, col):
    a = math.radians(a_deg)
    dx, dy = math.cos(a), math.sin(a)
    nx, ny = -dy * w / 2, dx * w / 2
    d.polygon([(cx + r0 * dx + nx, cy + r0 * dy + ny),
               (cx + r1 * dx + nx, cy + r1 * dy + ny),
               (cx + r1 * dx - nx, cy + r1 * dy - ny),
               (cx + r0 * dx - nx, cy + r0 * dy - ny)], fill=col)


def text_c(d, x, y, t, f, col):
    """Text centred on (x, y) by its ink box, not its advance box."""
    bb = d.textbbox((0, 0), t, font=f)
    d.text((x - (bb[2] + bb[0]) / 2, y - (bb[3] + bb[1]) / 2), t, font=f,
           fill=col)


def finish(img, w, h):
    return img.resize((w, h), Image.LANCZOS)


# ----------------------------------------------------------------- builders
def build_big(vmax, major, minor, label_div, red_from=None):
    img, d = canvas(BIG, BIG)
    c = u(BIG / 2)

    def ang(v):
        return BIG_SWEEP_START + v / vmax * BIG_SWEEP

    if red_from is not None:
        # red zone as a band behind the ticks, the ticks stay white on it
        arc_poly(d, c, c, u(BIG_R_TICK - BIG_TICK_MAJ), u(BIG_R_TICK + 2),
                 ang(red_from), ang(vmax), RED)

    n = vmax // minor
    for i in range(n + 1):
        v = i * minor
        is_major = v % major == 0
        ln = BIG_TICK_MAJ if is_major else BIG_TICK_MIN
        w = 4.6 if is_major else 2.2
        radial_line(d, c, c, u(BIG_R_TICK), u(BIG_R_TICK - ln), ang(v),
                    u(w), WHITE)

    # Each number is pushed out until its ink box clears the tick ring by the
    # same gap, so wide numbers at 3 and 9 o'clock do not touch their ticks.
    f = font(BIG_LABEL_PX)
    for v in range(0, vmax + 1, major):
        t = str(v // label_div)
        bb = d.textbbox((0, 0), t, font=f)
        a = math.radians(ang(v))
        half = (abs(math.cos(a)) * (bb[2] - bb[0]) +
                abs(math.sin(a)) * (bb[3] - bb[1])) / 2
        x, y = pt(c, c, u(BIG_R_TICK + BIG_LABEL_GAP) + half, ang(v))
        text_c(d, x, y, t, f, WHITE)

    return finish(img, BIG, BIG)


def build_temp(vmin, vmax, red_from):
    img, d = canvas(TEMP_W, TEMP_H)
    cx, cy = u(TEMP_W / 2), u(TEMP_H / 2)
    a0, a1 = TEMP_SWEEP_START, TEMP_SWEEP_START + TEMP_SWEEP

    def ang(v):
        return a0 + (v - vmin) / (vmax - vmin) * TEMP_SWEEP

    arc_poly(d, cx, cy, u(TEMP_R + 3), u(TEMP_R + 11), ang(red_from), a1, RED)
    arc_poly(d, cx, cy, u(TEMP_R - 1.2), u(TEMP_R + 1.2), a0, a1, LINE)
    for a in (a0, a1):
        radial_line(d, cx, cy, u(TEMP_R - 9), u(TEMP_R + 1), a, u(3.2), LINE)
    # middle mark
    radial_line(d, cx, cy, u(TEMP_R - 6), u(TEMP_R), (a0 + a1) / 2, u(2.2),
                LINE)

    # Min and max hang just under the ends of the arc. Anywhere along the end
    # radius the needle would lie across them when it rests on the stop.
    f = font(14)
    for v, a, side in ((vmin, a0, 1), (vmax, a1, -1)):
        x, y = pt(cx, cy, u(TEMP_R), a)
        text_c(d, x + side * u(5), y + u(17), str(v), f, WHITE_DIM)

    return finish(img, TEMP_W, TEMP_H)


def build_turbo(vmin, vmax, zones, mark):
    img, d = canvas(TURBO_W, TURBO_H)
    cx, cy = u(TURBO_W / 2), u(TURBO_H / 2)
    a0, a1 = TURBO_SWEEP_START, TURBO_SWEEP_START + TURBO_SWEEP

    def ang(v):
        return a0 + (v - vmin) / (vmax - vmin) * TURBO_SWEEP

    arc_poly(d, cx, cy, u(TURBO_R - 1.2), u(TURBO_R + 1.2), a0, a1, LINE)
    for lo, hi, col in zones:
        arc_poly(d, cx, cy, u(TURBO_R - 1.2), u(TURBO_R + TURBO_ZONE_W),
                 ang(lo), ang(hi), col)
    radial_line(d, cx, cy, u(TURBO_R - 8), u(TURBO_R + 1), ang(mark), u(2.6),
                LINE)
    return finish(img, TURBO_W, TURBO_H)


def build_needle(length, w_pivot, w_tip, tail):
    """Needle pointing RIGHT, which is what lv_meter_add_needle_img expects at
    scale angle zero. Returns the image and its pivot."""
    pad = 3
    w = tail + length + pad * 2
    h = int(math.ceil(w_pivot + 2 * pad)) | 1
    img, d = canvas(w, h, "RGBA")
    cy = u(h / 2)
    px = u(pad + tail)
    tip = u(pad + tail + length)
    tl = u(pad)

    def poly(inset, col):
        s = inset * SS
        d.polygon([(tip - s, cy - u(w_tip / 2) + s * 0.3),
                   (tip - s, cy + u(w_tip / 2) - s * 0.3),
                   (px, cy + u(w_pivot / 2) - s),
                   (tl + s, cy + u(w_pivot / 2 * 0.7) - s),
                   (tl + s, cy - u(w_pivot / 2 * 0.7) + s),
                   (px, cy - u(w_pivot / 2) + s)], fill=col + (255,))

    poly(0, NEEDLE_EDGE)
    poly(1.0, NEEDLE)
    return finish(img, w, h), (pad + tail, h // 2)


def build_hub(size):
    """Grey metal cap: dark rim, light-to-dark diagonal gradient, small
    highlight. Drawn per pixel at supersampled size, it is tiny."""
    ps = u(size)
    img = Image.new("RGBA", (ps, ps), (0, 0, 0, 0))
    px = img.load()
    c = ps / 2
    r_out = ps / 2 - SS
    r_rim = r_out - u(max(1.5, size / 16))
    for y in range(ps):
        for x in range(ps):
            dx, dy = x + 0.5 - c, y + 0.5 - c
            r = math.hypot(dx, dy)
            if r > r_out:
                continue
            if r > r_rim:
                col = (0x2A, 0x2B, 0x2D)
            else:
                t = ((dx + dy) / (2 * r_rim) + 1) / 2     # 0 top-left, 1 bottom-right
                g = int(0xB0 - 0x70 * t)
                hl = max(0.0, 1 - math.hypot(dx + r_rim * 0.35,
                                             dy + r_rim * 0.35) / (r_rim * 0.5))
                g = min(255, int(g + 0x30 * hl))
                col = (g, g, min(255, g + 4))
            px[x, y] = col + (255,)
    return img.resize((size, size), Image.LANCZOS)


# ------------------------------------------------------------------- emit
def emit_rgb565(name, img, out):
    w, h = img.size
    px = list(img.convert("RGB").getdata())
    out.append(f"static const uint8_t {name}_map[{w*h*2}] = {{")
    for r in range(h):
        row = []
        for c in range(w):
            red, g, b = px[r * w + c]
            v = ((red & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            row += [v & 0xFF, (v >> 8) & 0xFF]
        out.append("    " + " ".join(f"0x{v:02x}," for v in row))
    out.append("};")
    out.append(f"const lv_img_dsc_t {name} = {{")
    out.append("    .header = { .cf = LV_IMG_CF_TRUE_COLOR, .always_zero = 0,"
               f" .reserved = 0, .w = {w}, .h = {h} }},")
    out.append(f"    .data_size = {w*h*2},")
    out.append(f"    .data = {name}_map,")
    out.append("};")
    out.append("")
    return w * h * 2


def emit_rgba(name, img, out):
    w, h = img.size
    px = list(img.convert("RGBA").getdata())
    out.append(f"static const uint8_t {name}_map[{w*h*3}] = {{")
    for r in range(h):
        row = []
        for c in range(w):
            red, g, b, a = px[r * w + c]
            v = ((red & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            row += [v & 0xFF, (v >> 8) & 0xFF, a]
        out.append("    " + " ".join(f"0x{v:02x}," for v in row))
    out.append("};")
    out.append(f"const lv_img_dsc_t {name} = {{")
    out.append("    .header = { .cf = LV_IMG_CF_TRUE_COLOR_ALPHA,"
               f" .always_zero = 0, .reserved = 0, .w = {w}, .h = {h} }},")
    out.append(f"    .data_size = {w*h*3},")
    out.append(f"    .data = {name}_map,")
    out.append("};")
    out.append("")
    return w * h * 3


def main():
    art = {
        "dial_speed": build_big(SPEED_MAX, SPEED_MAJOR, SPEED_MINOR, 1),
        "dial_rpm": build_big(RPM_MAX, RPM_MAJOR, RPM_MINOR, 1000,
                              red_from=RPM_REDLINE),
        "dial_clt": build_temp(CLT_MIN, CLT_MAX, CLT_RED),
        "dial_iat": build_temp(IAT_MIN, IAT_MAX, IAT_RED),
        "dial_boost": build_turbo(BOOST_MIN, BOOST_MAX, BOOST_ZONES,
                                  BOOST_MARK),
        "dial_afr": build_turbo(AFR_MIN, AFR_MAX, AFR_ZONES, AFR_MARK),
    }
    needles = {
        "needle_big": build_needle(BIG_NEEDLE_LEN, 10, 3, 18),
        "needle_temp": build_needle(TEMP_NEEDLE_LEN, 6, 2, 9),
        "needle_turbo": build_needle(TURBO_NEEDLE_LEN, 6, 2, 9),
    }
    hubs = {
        "hub_big": build_hub(BIG_HUB),
        "hub_temp": build_hub(TEMP_HUB),
        "hub_turbo": build_hub(TURBO_HUB),
    }

    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.join(here, "..")

    out = ["/* Generated by tools/gen_dials.py - do not edit by hand.",
           " * Static gauge artwork, pre-rendered at 4x and downsampled.",
           " */",
           '#include "ui.h"', ""]
    total = 0
    for name, img in art.items():
        total += emit_rgb565(name, img, out)
    for name, (img, _) in needles.items():
        total += emit_rgba(name, img, out)
    for name, img in hubs.items():
        total += emit_rgba(name, img, out)
    open(os.path.join(root, "main", "dials.c"), "w").write("\n".join(out))

    h = ["/* Generated by tools/gen_dials.py - do not edit by hand.",
         " * Geometry of the artwork in dials.c. Every image has its pivot in",
         " * its centre; angles are LVGL's, 0 = 3 o'clock, clockwise.",
         " */",
         "#pragma once",
         '#include "lvgl.h"',
         ""]
    for name in art:
        h.append(f"extern const lv_img_dsc_t {name};")
    for name in list(needles) + list(hubs):
        h.append(f"extern const lv_img_dsc_t {name};")
    h.append("")

    def define(k, v):
        h.append(f"#define {k:<24} {v}")

    define("BIG_SWEEP_START", BIG_SWEEP_START)
    define("BIG_SWEEP", BIG_SWEEP)
    define("BIG_SIZE", BIG)
    define("SPEED_MAX", SPEED_MAX)
    define("RPM_MAX", RPM_MAX)
    define("RPM_REDLINE", RPM_REDLINE)
    h.append("")
    define("TEMP_SWEEP_START", TEMP_SWEEP_START)
    define("TEMP_SWEEP", TEMP_SWEEP)
    define("CLT_MIN", CLT_MIN)
    define("CLT_MAX", CLT_MAX)
    define("IAT_MIN", IAT_MIN)
    define("IAT_MAX", IAT_MAX)
    h.append("")
    define("TURBO_SWEEP_START", TURBO_SWEEP_START)
    define("TURBO_SWEEP", TURBO_SWEEP)
    define("BOOST_MIN", f"({BOOST_MIN:.2f}f)")
    define("BOOST_MAX", f"({BOOST_MAX:.2f}f)")
    define("AFR_MIN", f"({AFR_MIN:.2f}f)")
    define("AFR_MAX", f"({AFR_MAX:.2f}f)")
    h.append("")
    for name, (img, piv) in needles.items():
        define(f"{name.upper()}_PIVOT_X", piv[0])
        define(f"{name.upper()}_PIVOT_Y", piv[1])
    open(os.path.join(root, "main", "dials.h"), "w").write("\n".join(h) + "\n")

    d = os.path.join(root, "build_art")
    os.makedirs(d, exist_ok=True)
    # drop what older versions of this script wrote; logo.png belongs to
    # gen_assets.py and stays
    for f in ("needle_y.png", "needle_r.png", "hub.png"):
        if os.path.exists(os.path.join(d, f)):
            os.remove(os.path.join(d, f))
    for name, img in art.items():
        img.save(os.path.join(d, name + ".png"))
    for name, (img, _) in needles.items():
        img.save(os.path.join(d, name + ".png"))
    for name, img in hubs.items():
        img.save(os.path.join(d, name + ".png"))

    print(f"wrote main/dials.c, main/dials.h, build_art/  "
          f"({total / 1024:.0f} kB of image data)")


if __name__ == "__main__":
    main()
