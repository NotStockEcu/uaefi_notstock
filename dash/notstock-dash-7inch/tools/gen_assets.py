#!/usr/bin/env python3
"""Convert the supplied artwork into main/icons.c and main/logo.c.

Two sources, both under assets/:

  icons_sheet.png   the five card icons, yellow ink on black
  mockup.jpg        the original dash mockup, cropped for the wordmark

Both are traced by colour rather than by luminance: the icons are pulled out
with (R - B), which keeps the yellow strokes and drops the white labels, and
the wordmark keeps its real colours so NOT stays white and STOCK stays yellow.

Icons are not squared off. The oil can compositions are twice as wide as they
are tall, and forcing them into a square box would halve the drawing. They are
fitted into a 44x30 slot instead, and ui.c places every icon by the centre
read from its own header, so mixed sizes need no layout edits.

Run from the project root:  python tools/gen_assets.py
"""
import math
import os

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.join(HERE, "..", "assets")
MAIN = os.path.join(HERE, "..", "main")

ICON_BOX = (44, 30)          # max width, max height for a card icon

# x ranges of the five cards in the sheet, and the icon row
CARDS = [(48, 333), (489, 736), (923, 1144), (1354, 1520), (1775, 1986)]
ICON_ROWS = (190, 300)
NAMES = ["ic_water", "ic_oiltemp", "ic_oilpress", "ic_iat", "ic_fuel"]

# wordmark crop in the mockup: y offset from 86 % of the height, then the
# bounding box of anything taller than the flanking rules
LOGO_Y = 0.86
LOGO_BOX = (622, 352, 32, 67)   # x, width, y0, y1 within the band
LOGO_W = 240                 # rendered width in the bottom bar


def fit(w, h, box):
    s = min(box[0] / w, box[1] / h)
    return max(1, int(round(w * s))), max(1, int(round(h * s)))


def emit_alpha(name, img, out):
    w, h = img.size
    data = list(img.getchannel("A").getdata())
    out.append(f"static const uint8_t {name}_map[{w*h}] = {{")
    for r in range(h):
        out.append("    " + " ".join(f"0x{v:02x},"
                                     for v in data[r * w:(r + 1) * w]))
    out.append("};")
    out.append(f"const lv_img_dsc_t {name} = {{")
    out.append("    .header = { .cf = LV_IMG_CF_ALPHA_8BIT, .always_zero = 0,"
               f" .reserved = 0, .w = {w}, .h = {h} }},")
    out.append(f"    .data_size = {w*h},")
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
            red, g, b, al = px[r * w + c]
            v = ((red & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            row += [v & 0xFF, (v >> 8) & 0xFF, al]
        out.append("    " + " ".join(f"0x{v:02x}," for v in row))
    out.append("};")
    out.append(f"const lv_img_dsc_t {name} = {{")
    out.append("    .header = { .cf = LV_IMG_CF_TRUE_COLOR_ALPHA,"
               f" .always_zero = 0, .reserved = 0, .w = {w}, .h = {h} }},")
    out.append(f"    .data_size = {w*h*3},")
    out.append(f"    .data = {name}_map,")
    out.append("};")
    out.append("")


# ------------------------------------------------------------------------
# The supplied sheet only covers the five card icons. FAN and ALS live in the
# bottom bar and are drawn here, at 10x and downsampled, with the same stroke
# weight so they sit next to the traced ones without looking out of place.
SS, GRID, DRAWN = 10, 24.0, 30


def _draw(fn):
    p = int(GRID * SS)
    img = Image.new("L", (p, p), 0)
    fn(ImageDraw.Draw(img), lambda v: v * SS)
    out = Image.new("RGBA", (p, p), (255, 255, 255, 0))
    out.putalpha(img)
    return out.resize((DRAWN, DRAWN), Image.LANCZOS)


def _fan(d, u):
    cx = cy = 12.0
    for a in (0, 90, 180, 270):
        blade = []
        for i in range(33):
            t = 2 * math.pi * i / 32
            bx, by = 2.7 * math.cos(t), 4.6 * math.sin(t) - 5.5
            ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
            blade.append((u(cx + bx * ca - by * sa), u(cy + bx * sa + by * ca)))
        d.polygon(blade, fill=255)
    d.ellipse([u(cx - 3.2), u(cy - 3.2), u(cx + 3.2), u(cy + 3.2)], fill=0)
    d.ellipse([u(cx - 2.3), u(cy - 2.3), u(cx + 2.3), u(cy + 2.3)], fill=255)


def _flame(d, u):
    pts = [(12.0, 1.2), (15.4, 5.6), (17.4, 10.2), (17.6, 14.6),
           (15.6, 19.2), (12.0, 22.4), (8.4, 19.2), (6.4, 14.6),
           (6.6, 10.2), (8.6, 5.6)]
    d.polygon([(u(x), u(y)) for x, y in pts], fill=255)
    inner = [(12.0, 9.6), (14.1, 13.4), (13.9, 17.2), (12.0, 19.6),
             (10.1, 17.2), (9.9, 13.4)]
    d.polygon([(u(x), u(y)) for x, y in inner], fill=0)


def build_icons():
    src = np.array(Image.open(os.path.join(ASSETS, "icons_sheet.png"))
                   .convert("RGB"), float)
    # yellow ink survives, white labels cancel out because R == B there
    alpha = np.clip((src[..., 0] - src[..., 2]) / 150.0, 0, 1)

    out = ["/* Generated by tools/gen_assets.py from assets/icons_sheet.png.",
           " * ALPHA_8BIT, coloured at runtime with img_recolor. */",
           '#include "ui.h"', ""]
    png_dir = os.path.join(HERE, "..", "icons_png")
    os.makedirs(png_dir, exist_ok=True)

    for (x0, x1), name in zip(CARDS, NAMES):
        sub = alpha[ICON_ROWS[0]:ICON_ROWS[1], x0:x1 + 1]
        m = sub > 0.2
        xs = np.nonzero(m.any(0))[0]
        ys = np.nonzero(m.any(1))[0]
        sub = sub[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
        h, w = sub.shape

        big = Image.new("RGBA", (w, h), (255, 255, 255, 0))
        big.putalpha(Image.fromarray((sub * 255).astype("uint8")))
        nw, nh = fit(w, h, ICON_BOX)
        small = big.resize((nw, nh), Image.LANCZOS)

        emit_alpha(name, small, out)
        big.resize((w * 2, h * 2), Image.LANCZOS).save(
            os.path.join(png_dir, name + ".png"))
        print(f"  {name}: source {w}x{h} -> {nw}x{nh}")

    for name, fn in (("ic_fan", _fan), ("ic_flame", _flame)):
        img = _draw(fn)
        emit_alpha(name, img, out)
        img.resize((192, 192), Image.LANCZOS).save(
            os.path.join(png_dir, name + ".png"))
        print(f"  {name}: drawn -> {DRAWN}x{DRAWN}")

    open(os.path.join(MAIN, "icons.c"), "w").write("\n".join(out))
    print("wrote main/icons.c")


def build_logo():
    src = Image.open(os.path.join(ASSETS, "mockup.jpg")).convert("RGB")
    a = np.array(src, float)
    y0 = int(src.height * LOGO_Y)
    x0, x1, ry0, ry1 = LOGO_BOX
    crop = a[y0 + ry0:y0 + ry1, x0:x0 + x1]

    lum = 0.299 * crop[..., 0] + 0.587 * crop[..., 1] + 0.114 * crop[..., 2]
    alpha = np.clip((lum - 26) / 140.0, 0, 1)

    # the mockup is a JPEG, so snap each pixel to the two brand colours
    # instead of carrying compression mush into the panel
    is_yellow = (crop[..., 0] - crop[..., 2]) > 55
    rgb = np.zeros(crop.shape, "uint8")
    rgb[..., 0] = np.where(is_yellow, 0xF5, 0xFF)
    rgb[..., 1] = np.where(is_yellow, 0xC5, 0xFF)
    rgb[..., 2] = np.where(is_yellow, 0x18, 0xFF)

    img = Image.fromarray(rgb, "RGB").convert("RGBA")
    img.putalpha(Image.fromarray((alpha * 255).astype("uint8")))

    h = max(1, int(round(img.height * LOGO_W / img.width)))
    img = img.resize((LOGO_W, h), Image.LANCZOS)

    out = ["/* Generated by tools/gen_assets.py from the dash mockup.",
           " * TRUE_COLOR_ALPHA so NOT stays white and STOCK stays yellow. */",
           '#include "ui.h"', ""]
    emit_rgba("logo_notstock", img, out)
    open(os.path.join(MAIN, "logo.c"), "w").write("\n".join(out))
    img.save(os.path.join(HERE, "..", "build_art", "logo.png"))
    print(f"wrote main/logo.c, wordmark {LOGO_W}x{h}, "
          f"{LOGO_W*h*3/1024:.0f} kB")


if __name__ == "__main__":
    os.makedirs(os.path.join(HERE, "..", "build_art"), exist_ok=True)
    build_icons()
    build_logo()
