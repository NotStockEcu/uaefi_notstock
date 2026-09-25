#!/usr/bin/env python3
"""Pre-render the static gauge artwork into LVGL C arrays.

The dial face never changes, so there is no reason to rebuild it out of LVGL
primitives every frame. Rendering it here with 4x supersampling buys radial
gradients, soft bevels, hairline ticks and properly hinted labels, none of
which LVGL 8 can draw on its own. The firmware then only has to move a needle
and rewrite a number.

Outputs main/dials.c:
    dial_boost, dial_afr    RGB565 face images
    needle_y, needle_r      RGBA needle images (yellow / red)
    hub_cap                 RGBA centre cap

Run from the project root:  python tools/gen_dials.py
"""
import math
import os

from PIL import Image, ImageDraw, ImageFont, ImageFilter

SS = 4                       # supersample factor
SIZE = 202                   # final dial size, matches LY_METER
P = SIZE * SS

DJ = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"

# radii in final pixels, mirrored by the LY_* constants in ui.c
R_FACE = 99                  # outer edge of the dial disc
R_BAND_OUT = 98              # coloured band outer edge
BAND_W = 7
R_TICK_OUT = R_BAND_OUT - BAND_W - 3
TICK_MAJ, TICK_MIN = 14, 7
R_LABEL = R_TICK_OUT - TICK_MAJ - 12

SWEEP_START, SWEEP = 145, 250

Y = (0xF5, 0xC5, 0x18)
Y_DEEP = (0x8A, 0x6F, 0x0C)
RED = (0xE2, 0x24, 0x24)
RED_DEEP = (0x6E, 0x12, 0x12)
GREY = (0x5A, 0x5F, 0x64)
GREY_DEEP = (0x2E, 0x31, 0x35)
TICK_C = (0xF2, 0xF2, 0xF2)
TICK_MINC = (0x7A, 0x7F, 0x84)
LABEL_C = (0xDA, 0xDD, 0xE0)


def u(v):
    return int(round(v * SS))


def arc_poly(d, cx, cy, r_in, r_out, a0, a1, col, steps=None):
    """Filled annular sector. PIL's arc() has no antialiasing and a single
    width, so bands get drawn as polygons at supersampled resolution."""
    if a1 < a0:
        a0, a1 = a1, a0
    steps = steps or max(8, int((a1 - a0) * 2))
    pts = []
    for i in range(steps + 1):
        a = math.radians(a0 + (a1 - a0) * i / steps)
        pts.append((cx + r_out * math.cos(a), cy + r_out * math.sin(a)))
    for i in range(steps, -1, -1):
        a = math.radians(a0 + (a1 - a0) * i / steps)
        pts.append((cx + r_in * math.cos(a), cy + r_in * math.sin(a)))
    d.polygon(pts, fill=col)


def radial_face(size, r_out, inner, outer):
    """Disc with a centre-to-edge radial gradient. Built as a coarse ramp of
    concentric discs, then blurred, which is far cheaper than per-pixel work
    and indistinguishable once downsampled."""
    img = Image.new("RGB", (size, size), (0, 0, 0))
    d = ImageDraw.Draw(img)
    c = size / 2
    steps = 48
    for i in range(steps, 0, -1):
        f = i / steps
        r = r_out * f
        col = tuple(int(outer[k] + (inner[k] - outer[k]) * (1 - f) ** 1.4)
                    for k in range(3))
        d.ellipse([c - r, c - r, c + r, c + r], fill=col)
    return img.filter(ImageFilter.GaussianBlur(size / 90))


def build_dial(name, vmin, vmax, majors, minors, zones, tick_dec):
    cx = cy = P / 2

    face = radial_face(P, u(R_FACE), (0x1E, 0x20, 0x23), (0x05, 0x05, 0x06))
    mask = Image.new("L", (P, P), 0)
    ImageDraw.Draw(mask).ellipse(
        [cx - u(R_FACE), cy - u(R_FACE), cx + u(R_FACE), cy + u(R_FACE)],
        fill=255)
    img = Image.new("RGB", (P, P), (0, 0, 0))
    img.paste(face, (0, 0), mask)
    d = ImageDraw.Draw(img)

    # hairline bevel around the face
    d.ellipse([cx - u(R_FACE), cy - u(R_FACE), cx + u(R_FACE), cy + u(R_FACE)],
              outline=(0x2C, 0x2F, 0x33), width=u(1.0))

    def ang(v):
        return SWEEP_START + (v - vmin) / (vmax - vmin) * SWEEP

    # unlit track, then the coloured zones over it
    arc_poly(d, cx, cy, u(R_BAND_OUT - BAND_W), u(R_BAND_OUT),
             SWEEP_START, SWEEP_START + SWEEP, (0x1B, 0x1D, 0x20))
    for lo, hi, col in zones:
        arc_poly(d, cx, cy, u(R_BAND_OUT - BAND_W), u(R_BAND_OUT),
                 ang(lo), ang(hi), col)

    # ticks
    n = (majors - 1) * minors
    for i in range(n + 1):
        v = vmin + (vmax - vmin) * i / n
        a = math.radians(ang(v))
        major = i % minors == 0
        ln = TICK_MAJ if major else TICK_MIN
        w = 3.0 if major else 1.4
        col = TICK_C if major else TICK_MINC
        r0, r1 = u(R_TICK_OUT), u(R_TICK_OUT - ln)
        dx, dy = math.cos(a), math.sin(a)
        nx, ny = -dy * u(w) / 2, dx * u(w) / 2
        d.polygon([(cx + r0 * dx + nx, cy + r0 * dy + ny),
                   (cx + r1 * dx + nx, cy + r1 * dy + ny),
                   (cx + r1 * dx - nx, cy + r1 * dy - ny),
                   (cx + r0 * dx - nx, cy + r0 * dy - ny)], fill=col)

    # labels
    f = ImageFont.truetype(DJ, u(17))
    for k in range(majors):
        v = vmin + (vmax - vmin) * k / (majors - 1)
        a = math.radians(ang(v))
        t = f"{v:.1f}" if tick_dec == 1 else f"{v:.0f}"
        lx = cx + u(R_LABEL) * math.cos(a)
        ly = cy + u(R_LABEL) * math.sin(a)
        bb = d.textbbox((0, 0), t, font=f)
        d.text((lx - (bb[2] - bb[0]) / 2 - bb[0],
                ly - (bb[3] - bb[1]) / 2 - bb[1]), t, font=f, fill=LABEL_C)

    return img.resize((SIZE, SIZE), Image.LANCZOS)


def build_needle(col_main, col_edge):
    """Needle pointing RIGHT, which is what lv_meter_add_needle_img expects at
    scale angle zero. Pivot sits near the left end so the short tail balances
    the long taper."""
    w, h = 108, 26
    pw, ph = w * SS, h * SS
    img = Image.new("RGBA", (pw, ph), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cy = ph / 2
    piv_x = 18 * SS
    tip_x = 104 * SS   # R_TICK_OUT is 88, pivot is 18, so 104-18 = 86
    tail_x = 5 * SS

    outer = [(tip_x, cy - 1.7 * SS), (tip_x, cy + 1.7 * SS),
             (piv_x, cy + 4.8 * SS), (tail_x, cy + 3.1 * SS),
             (tail_x, cy - 3.1 * SS), (piv_x, cy - 4.8 * SS)]
    d.polygon(outer, fill=col_edge + (255,))
    inner = [(tip_x - 2 * SS, cy - 0.85 * SS), (tip_x - 2 * SS, cy + 0.85 * SS),
             (piv_x, cy + 3.5 * SS), (tail_x + 2 * SS, cy + 2.0 * SS),
             (tail_x + 2 * SS, cy - 2.0 * SS), (piv_x, cy - 3.5 * SS)]
    d.polygon(inner, fill=col_main + (255,))

    return img.resize((w, h), Image.LANCZOS), (18, h // 2)


def build_hub():
    s = 34
    ps = s * SS
    img = Image.new("RGBA", (ps, ps), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = ps / 2
    for r, col in ((16, (0x3E, 0x41, 0x45)), (14.5, (0x17, 0x18, 0x1A)),
                   (9, (0x0A, 0x0A, 0x0B)), (3.2, (0x2A, 0x2C, 0x2F))):
        rr = r * SS
        d.ellipse([c - rr, c - rr, c + rr, c + rr], fill=col + (255,))
    return img.resize((s, s), Image.LANCZOS)


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


def main():
    boost = build_dial(
        "boost", -1.0, 2.0, 7, 5,
        [(-1.0, 0.0, GREY), (0.0, 1.2, Y_DEEP), (1.2, 2.0, RED)], 1)
    afr = build_dial(
        "afr", 10.0, 18.0, 9, 5,
        [(10.0, 11.5, Y_DEEP), (11.5, 16.0, GREY_DEEP), (16.0, 18.0, RED)], 0)

    ny, piv = build_needle(Y, (0x6B, 0x55, 0x08))
    nr, _ = build_needle(RED, (0x5E, 0x0F, 0x0F))
    print(f"needle pivot for lv_meter_add_needle_img: {piv}")
    hub = build_hub()

    out = ["/* Generated by tools/gen_dials.py - do not edit by hand.",
           " * Static gauge artwork, pre-rendered at 4x and downsampled.",
           " */",
           '#include "ui.h"', ""]
    emit_rgb565("dial_boost", boost, out)
    emit_rgb565("dial_afr", afr, out)
    emit_rgba("needle_y", ny, out)
    emit_rgba("needle_r", nr, out)
    emit_rgba("hub_cap", hub, out)

    here = os.path.dirname(os.path.abspath(__file__))
    path = os.path.join(here, "..", "main", "dials.c")
    open(path, "w").write("\n".join(out))

    kb = (boost.size[0] * boost.size[1] * 2 * 2
          + ny.size[0] * ny.size[1] * 3 * 2
          + hub.size[0] * hub.size[1] * 3) / 1024
    print(f"wrote {os.path.normpath(path)}")
    print(f"dial {boost.size[0]}x{boost.size[1]}  needle {ny.size}  "
          f"pivot {piv}  hub {hub.size}")
    print(f"flash cost {kb:.0f} kB")

    # PNGs for eyeballing and for tools/preview.py
    d = os.path.join(here, "..", "build_art")
    os.makedirs(d, exist_ok=True)
    boost.save(os.path.join(d, "dial_boost.png"))
    afr.save(os.path.join(d, "dial_afr.png"))
    ny.save(os.path.join(d, "needle_y.png"))
    nr.save(os.path.join(d, "needle_r.png"))
    hub.save(os.path.join(d, "hub.png"))


if __name__ == "__main__":
    main()
