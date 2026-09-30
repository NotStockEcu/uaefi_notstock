#!/usr/bin/env python3
"""Mock-up: the round gauge in the left outer air vent of a VW T5.1, held by
a 3D printed grille. Stylised, not measured: the vent is a rounded rectangle
about 95 x 80 mm, the display's active area 53 mm across.

Writes preview/mockup-t5-front-<look>.png, preview/mockup-t5-driver.png and
preview/mockup-t5-looks.png, from the gauge previews (run tools/preview.py
first).

Run from the project root:  python tools/mockup_t5.py
"""
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PREV = os.path.join(ROOT, "preview")

PX_MM = 7.5                      # pixels per millimetre
W, H = 1400, 1000
VENT = (95, 80, 14)              # vent opening: width, height, corner radius (mm)
ACTIVE = 53.3                    # display active area, diameter (mm)
BOSS = 70                        # the grille's round boss around the display (mm)


def mm(v):
    return int(round(v * PX_MM))


def noise(w, h, amount, blur, seed):
    rng = np.random.default_rng(seed)
    n = rng.normal(0, 1, (h, w)).astype(np.float32)
    img = Image.fromarray(np.clip(128 + n * 40, 0, 255).astype(np.uint8), "L")
    img = img.filter(ImageFilter.GaussianBlur(blur))
    a = (np.asarray(img, dtype=np.float32) - 128) / 40 * amount
    return a


def dashboard():
    """Grey textured dash plastic, a soft curve of light across it."""
    y, x = np.mgrid[0:H, 0:W].astype(np.float32)
    base = 46 - 16 * ((y / H - 0.35) ** 2) * 4 + 6 * np.cos((x / W - 0.3) * 2.2)
    base += noise(W, H, 4.0, 0.8, 1) + noise(W, H, 3.0, 3.0, 2)
    rgb = np.stack([base, base * 1.01, base * 1.04], -1)
    img = Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8), "RGB")
    # a panel seam sweeping across above the vent, lit on its lower edge
    d = ImageDraw.Draw(img)
    pts = [(x, 70 + 40 * math.sin(x / W * math.pi * 0.9)) for x in range(0, W, 8)]
    d.line(pts, fill=(18, 18, 20), width=5)
    d.line([(x, y + 4) for x, y in pts], fill=(70, 72, 76), width=2)
    return img


def layer_lines(w, h, seed):
    """FDM print: fine horizontal layer lines on matte black."""
    y = np.arange(h, dtype=np.float32)[:, None]
    lines = 5 * np.sin(y / (0.2 * PX_MM) * math.pi) ** 2
    a = 24 + lines + noise(w, h, 2.0, 0.6, seed)
    return a.astype(np.float32) * np.ones((1, w), np.float32)


def gauge(look, d):
    name = {"notstock": "boost", "retro": "retro-boost",
            "futuro": "futuro-boost"}[look]
    img = Image.open(os.path.join(PREV, name + ".png")).convert("RGB")
    img = img.resize((d, d), Image.LANCZOS)
    m = Image.new("L", (d, d), 0)
    ImageDraw.Draw(m).ellipse([0, 0, d - 1, d - 1], fill=255)
    return img, m


def compose(look):
    img = dashboard()
    cx, cy = W // 2, H // 2 + 10
    vw, vh, vr = mm(VENT[0]), mm(VENT[1]), mm(VENT[2])
    box = [cx - vw // 2, cy - vh // 2, cx + vw // 2, cy + vh // 2]

    # the dash's own vent surround: a raised lip with light from above
    lip = mm(4)
    sh = Image.new("L", (W, H), 0)
    ImageDraw.Draw(sh).rounded_rectangle(
        [box[0] - lip + 6, box[1] - lip + 10, box[2] + lip + 6, box[3] + lip + 10],
        radius=vr + lip, fill=150)
    img.paste((8, 8, 9), (0, 0), sh.filter(ImageFilter.GaussianBlur(14)))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([box[0] - lip, box[1] - lip, box[2] + lip, box[3] + lip],
                        radius=vr + lip, fill=(58, 60, 64))
    d.rounded_rectangle([box[0] - lip + 3, box[1] - lip + 3, box[2] + lip - 3,
                         box[3] + lip - 1], radius=vr + lip, fill=(40, 42, 45))

    # the printed grille fills the opening
    gw, gh = box[2] - box[0], box[3] - box[1]
    grille = layer_lines(gw, gh, 3)
    g_img = Image.fromarray(np.stack([grille] * 3, -1).clip(0, 255)
                            .astype(np.uint8), "RGB")
    gd = ImageDraw.Draw(g_img)
    # slats: dark gaps between horizontal bars, with a lit top edge
    bar, gap = mm(6.5), mm(4.0)
    y = mm(3)
    while y < gh:
        gd.rectangle([0, y, gw, y + gap], fill=(6, 6, 7))
        gd.line([0, y + gap, gw, y + gap], fill=(52, 53, 56), width=2)
        y += bar + gap
    # a frame round the slats
    fr = mm(3)
    gm = Image.new("L", (gw, gh), 0)
    ImageDraw.Draw(gm).rounded_rectangle([0, 0, gw - 1, gh - 1], radius=vr,
                                         fill=255)
    inner = Image.new("L", (gw, gh), 0)
    ImageDraw.Draw(inner).rounded_rectangle([fr, fr, gw - fr, gh - fr],
                                            radius=max(1, vr - fr), fill=255)
    frame = Image.fromarray((layer_lines(gw, gh, 4) + 6).clip(0, 255)
                            .astype(np.uint8), "L").convert("RGB")
    g_img.paste(frame, (0, 0), Image.fromarray(
        np.asarray(gm) & ~np.asarray(inner)))
    img.paste(g_img, (box[0], box[1]), gm)

    # the round boss: shadow, chamfered ring, then the display
    bd = mm(BOSS)
    bs = Image.new("L", (W, H), 0)
    ImageDraw.Draw(bs).ellipse([cx - bd // 2 + 8, cy - bd // 2 + 14,
                                cx + bd // 2 + 8, cy + bd // 2 + 14], fill=190)
    img.paste((0, 0, 0), (0, 0), bs.filter(ImageFilter.GaussianBlur(16)))
    ring = Image.new("RGB", (bd, bd), (0, 0, 0))
    rr = layer_lines(bd, bd, 5)
    yy, xx = np.mgrid[0:bd, 0:bd].astype(np.float32)
    rad = np.hypot(xx - bd / 2, yy - bd / 2) / (bd / 2)
    ang = np.arctan2(yy - bd / 2, xx - bd / 2)
    # chamfer: lit on the upper left
    lit = np.clip((rad - 0.80) / 0.2, 0, 1) * (-np.sin(ang + math.pi / 4)) * 22
    ring = Image.fromarray(np.stack([rr + 6 + lit] * 3, -1).clip(0, 255)
                           .astype(np.uint8), "RGB")
    rm = Image.new("L", (bd, bd), 0)
    ImageDraw.Draw(rm).ellipse([0, 0, bd - 1, bd - 1], fill=255)
    img.paste(ring, (cx - bd // 2, cy - bd // 2), rm)

    # display: the module's black rim, then the active area
    dm = mm(ACTIVE + 3)
    d = ImageDraw.Draw(img)
    d.ellipse([cx - dm // 2, cy - dm // 2, cx + dm // 2, cy + dm // 2],
              fill=(4, 4, 5))
    ad = mm(ACTIVE)
    g, m = gauge(look, ad)
    img.paste(g, (cx - ad // 2, cy - ad // 2), m)

    # glass: a soft sheen over the upper left of the display
    sheen = Image.new("L", (W, H), 0)
    sd = ImageDraw.Draw(sheen)
    sd.ellipse([cx - dm // 2 - 30, cy - dm // 2 - 50, cx + dm // 6, cy - dm // 10],
               fill=42)
    clip = Image.new("L", (W, H), 0)
    ImageDraw.Draw(clip).ellipse([cx - dm // 2, cy - dm // 2, cx + dm // 2,
                                  cy + dm // 2], fill=255)
    sheen = Image.fromarray(np.minimum(np.asarray(sheen.filter(
        ImageFilter.GaussianBlur(30))), np.asarray(clip)))
    img.paste((255, 255, 255), (0, 0), sheen)
    return img


def driver_view(front):
    """The same, seen from the driver's seat: from the right and a little
    above, so the left vent leans away."""
    w, h = front.size
    # where the corners of the flat picture land (x, y)
    dst = [(150, 40), (w - 20, 120), (w - 20, h - 110), (150, h - 20)]
    src = [(0, 0), (w, 0), (w, h), (0, h)]
    coeffs = _perspective(dst, src)
    out = front.transform((w, h), Image.PERSPECTIVE, coeffs, Image.BICUBIC)
    # what the warp leaves empty is more dashboard, darker as it turns away
    cover = Image.new("L", (w, h), 255).transform(
        (w, h), Image.PERSPECTIVE, coeffs, Image.BICUBIC)
    back = Image.eval(dashboard(), lambda v: int(v * 0.8))
    back.paste(out, (0, 0), cover)
    return back


def _perspective(pa, pb):
    a = []
    for (x, y), (u, v) in zip(pa, pb):
        a.append([x, y, 1, 0, 0, 0, -u * x, -u * y])
        a.append([0, 0, 0, x, y, 1, -v * x, -v * y])
    A = np.array(a, dtype=np.float64)
    B = np.array(pb, dtype=np.float64).reshape(8)
    return np.linalg.solve(A, B).tolist()


def main():
    fronts = {}
    for look in ("notstock", "retro", "futuro"):
        f = compose(look)
        f.save(os.path.join(PREV, "mockup-t5-front-%s.png" % look))
        fronts[look] = f
    driver_view(fronts["notstock"]).save(os.path.join(PREV, "mockup-t5-driver.png"))
    sheet = Image.new("RGB", (W * 3 // 2, H // 2), (0, 0, 0))
    for i, look in enumerate(("notstock", "retro", "futuro")):
        sheet.paste(fronts[look].resize((W // 2, H // 2), Image.LANCZOS),
                    (i * W // 2, 0))
    sheet.save(os.path.join(PREV, "mockup-t5-looks.png"))
    print("wrote preview/mockup-t5-*.png")


if __name__ == "__main__":
    main()
