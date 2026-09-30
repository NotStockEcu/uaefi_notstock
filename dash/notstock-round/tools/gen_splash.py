#!/usr/bin/env python3
"""Boot screen logo for the round gauge: assets/splash.png -> main/splash.c
(the same badge and keying as the 7" dash's tools/gen_splash.py)

The owner's round NOT STOCK. / NOT STABLE. badge, shown on black at power-up,
filling the round panel.

The source has a white background. It is keyed out on the blue channel: the
badge is only yellow and black, both with blue near zero, while the white
background has blue at 255. So blue / 255 is how much of a pixel is
background, and scaling the pixel by (1 - that) turns the background black
and keeps the antialiased rim of the disc smooth instead of leaving a white
fringe. The result is cropped to the disc and scaled to SIZE.

To change the logo, replace assets/splash.png (square-ish, any size, on white
or on black) and run this again. Other boot logos (the settings' LOGO row)
are drawn here, see LOGOS; one for another car goes on that list and on
RND_LOGO_* in ui_round.h.

Run from the project root:  python tools/gen_splash.py
"""
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
SOURCE = os.path.join(ROOT, "assets", "splash.png")

SIZE = 456                   # rendered size on the 480x480 round panel


def load():
    src = Image.open(SOURCE)
    if src.mode in ("RGBA", "LA", "P"):
        # flatten any transparency onto white, the keying below expects it
        bg = Image.new("RGBA", src.size, (255, 255, 255, 255))
        src = Image.alpha_composite(bg, src.convert("RGBA"))
    a = np.asarray(src.convert("RGB"), dtype=np.float32)

    bgness = a[..., 2] / 255.0
    out = a * (1.0 - bgness[..., None])

    # crop to the disc, square, with a hair of margin
    ys, xs = np.nonzero(bgness < 0.5)
    cx, cy = (xs.min() + xs.max()) / 2, (ys.min() + ys.max()) / 2
    half = max(xs.max() - xs.min(), ys.max() - ys.min()) / 2 * 1.02
    img = Image.fromarray(out.clip(0, 255).astype(np.uint8), "RGB")
    img = img.crop((int(cx - half), int(cy - half),
                    int(cx + half), int(cy + half)))
    return img.resize((SIZE, SIZE), Image.LANCZOS)


def _offset_polyline(pts, hw):
    """Both sides of a thick polyline with mitred (sharp) joints, as one
    polygon: how the VW letters get their pointed tips."""
    pts = [np.array(p, dtype=float) for p in pts]
    left, right = [], []
    for i, p in enumerate(pts):
        if i == 0:
            d = pts[1] - p
            n = np.array([-d[1], d[0]]) / np.linalg.norm(d)
            left.append(p + n * hw)
            right.append(p - n * hw)
        elif i == len(pts) - 1:
            d = p - pts[i - 1]
            n = np.array([-d[1], d[0]]) / np.linalg.norm(d)
            left.append(p + n * hw)
            right.append(p - n * hw)
        else:
            d0 = (p - pts[i - 1]) / np.linalg.norm(p - pts[i - 1])
            d1 = (pts[i + 1] - p) / np.linalg.norm(pts[i + 1] - p)
            n0 = np.array([-d0[1], d0[0]])
            n1 = np.array([-d1[1], d1[0]])
            m = n0 + n1
            m /= np.linalg.norm(m)
            k = hw / max(np.dot(m, n0), 0.2)      # mitre length
            left.append(p + m * k)
            right.append(p - m * k)
    return [tuple(q) for q in left] + [tuple(q) for q in reversed(right)]


def _blur(a, radius):
    img = Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8), "L")
    img = img.filter(ImageFilter.GaussianBlur(radius))
    return np.asarray(img, dtype=np.float32) / 255.0


def vw():
    """VW roundel in the 2012-2019 style: chrome ring and letters with a
    bevel, on a glossy blue disc. Drawn, not traced: the shapes are the
    usual V over W; the chrome comes from the slope of the blurred shape
    (lit from the top left). For the owner's own T5; it is VW's trademark."""
    ss = 4
    n = SIZE * ss
    c = n / 2
    R = n / 2 * 0.985
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    dx, dy = (xx - c) / R, (yy - c) / R
    rr = np.sqrt(dx * dx + dy * dy)

    def aa(edge):                           # antialiased step at 0
        return np.clip(edge * R / 1.5 + 0.5, 0, 1)

    disc = aa(1.0 - rr)
    r_in = 0.845
    ring = disc * aa(rr - r_in)

    # letters: V over W, their ends running into the ring
    lm = Image.new("L", (n, n), 0)
    ld = ImageDraw.Draw(lm)

    def P(x, y):
        return (c + x * R, c + y * R)

    # mitred tips reach hw / sin(half angle) past their points, so the V's
    # apex sits well above the W's middle peak to keep a thin blue gap
    hw = 0.068
    V = [(-0.46, -1.02), (0.0, -0.13), (0.46, -1.02)]
    W = [(-1.00, -0.36), (-0.37, 0.80), (0.0, 0.33), (0.37, 0.80),
         (1.00, -0.36)]
    for poly in (V, W):
        ld.polygon([P(x, y) for x, y in _offset_polyline(poly, hw)],
                   fill=255)
    letters = np.asarray(lm, dtype=np.float32) / 255.0
    letters = np.minimum(letters, aa(0.99 - rr))
    chrome = np.maximum(ring, letters)

    # blue disc: lighter up top, a soft gloss over the upper half
    t = np.clip((dy + 1) / 2, 0, 1)
    top = np.array([60, 140, 215], dtype=np.float32)
    bot = np.array([6, 38, 96], dtype=np.float32)
    blue = top[None, None, :] * (1 - t[..., None]) + bot[None, None, :] * t[..., None]
    radial = np.clip(1.0 - 0.35 * rr, 0, 1)
    blue *= radial[..., None]
    gloss = np.clip(1.0 - np.sqrt((dx / 0.75) ** 2 + ((dy + 0.45) / 0.45) ** 2),
                    0, 1) ** 1.5 * 0.28
    blue = blue + (255 - blue) * gloss[..., None]

    # the chrome throws a shadow onto the blue
    sh = _blur(np.roll(np.roll(chrome, int(0.025 * R), 0), int(0.02 * R), 1),
               0.02 * R)
    blue *= (1 - 0.55 * sh)[..., None]

    # chrome: a reflected sky and horizon, bright above, a dark band just
    # under the middle, lighter again at the bottom; bevel from the slope
    base = np.interp(dy, [-1.0, -0.2, 0.02, 0.10, 0.45, 1.0],
                     [250, 215, 175, 95, 125, 185]).astype(np.float32)
    base += 12 * np.cos(dx * 2.5)
    bl = _blur(chrome, 0.018 * R)
    gy, gx = np.gradient(bl)
    light = (-gx - gy) / np.sqrt(2) * R * 0.018
    spec = np.clip(light, 0, 1) ** 2
    lum = base + 170 * light + 110 * spec
    lum = np.clip(lum, 40, 255)
    metal = np.stack([lum * 0.97, lum * 0.99, lum], axis=-1)

    rgb = blue * (1 - chrome[..., None]) + metal * chrome[..., None]
    rgb *= disc[..., None]
    img = Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8), "RGB")
    return img.resize((SIZE, SIZE), Image.LANCZOS)


# boot logos, in the order of RND_LOGO_* in ui_round.h
LOGOS = [("notstock", load), ("vw", vw)]


def c_image(name, img):
    w, h = img.size
    px = np.asarray(img, dtype=np.uint16)
    v = ((px[..., 0] & 0xF8) << 8) | ((px[..., 1] & 0xFC) << 3) | (px[..., 2] >> 3)
    out = [f"static const uint16_t {name}_map[{w*h}] = {{"]
    for row in v:
        out.append("    " + " ".join(f"0x{p:04x}," for p in row))
    out += ["};",
            f"const lv_img_dsc_t {name} = {{",
            "    .header = { .cf = LV_IMG_CF_TRUE_COLOR, .always_zero = 0,"
            f" .reserved = 0, .w = {w}, .h = {h} }},",
            f"    .data_size = {w*h*2},",
            f"    .data = (const uint8_t *){name}_map,",
            "};", ""]
    return out


def main():
    # uint16_t, not bytes: the pixels are read as 16-bit words, and a byte
    # array carries no alignment guarantee
    out = ["/* Generated by tools/gen_splash.py. Do not edit by hand.",
           " * Boot logos, RGB565, native byte order. */",
           '#include "lvgl.h"', ""]
    names = []
    for key, make in LOGOS:
        img = make()
        name = "boot_logo_" + key
        names.append(name)
        out += c_image(name, img)
        img.save(os.path.join(ROOT, "preview", "logo-%s.png" % key))
    out.append("const lv_img_dsc_t *const boot_logo[%d] = {" % len(LOGOS))
    out.append("    " + ", ".join("&" + n for n in names) + ",")
    out.append("};")
    open(os.path.join(ROOT, "main", "splash.c"), "w").write("\n".join(out) + "\n")
    print("wrote main/splash.c (%d logos, %d kB), preview/logo-*.png"
          % (len(LOGOS), len(LOGOS) * SIZE * SIZE * 2 // 1024))


if __name__ == "__main__":
    main()
