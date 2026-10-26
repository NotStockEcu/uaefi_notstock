"""The dials of the A4 gauge, after the Audi A4 B8 cluster at night: a black
face inside a thin red lit rim, short white dashes for the scale, big white
numerals (orange from the warn limit on, with an amber band inside the
ticks there), the scale from just right of the bottom over the left and
the top round to 3 o'clock as the rev counter's, the page's name and unit
in the free lower right where the cluster has its "1/min x1000". One per
page, plus the needle (red, lit), its black hub and the amber DPF
lamp, drawn live by LVGL.

Writes main/faces/a4_*.c (LVGL 8 images, RGB565; the live parts with
alpha) and preview/face-*.png. No maker's logo anywhere.
Run from the project root: python3 tools/gen_faces.py
"""
import math
import os
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

N = 466
C = N / 2
SS = 3                                   # supersampled, then shrunk
ROOT = os.path.join(os.path.dirname(__file__), "..")
FONT = os.path.join(ROOT, "assets", "fonts", "Barlow-%d-latin.ttf")

START = 95.0           # degrees, screen (clockwise from +x); also in ui_a4.c
SWEEP = 265.0
R_RIM = 225            # the red lit rim
R_TICK = 214           # outer end of the ticks
R_NUM = 172            # centre of the numerals, at most
R_BAND = (190, 197)    # the red band inside the ticks (ui_a4.c draws it)
WHITE = (238, 238, 236)
GREY = (160, 160, 160)

# id, name, unit, lo, hi, labels (value, text), minor ticks per gap, warn
PAGES = [
    ("oil",   "OLEJ",  "°C",  50, 150,
     [(50, "50"), (70, "70"), (90, "90"), (110, "110"), (130, "130"), (150, "150")], 3, 130),
    ("iat",   "SÁNÍ", "°C", -20, 80,
     [(-20, "-20"), (0, "0"), (20, "20"), (40, "40"), (60, "60"), (80, "80")], 3, 60),
    ("clt",   "VODA",  "°C",  50, 130,
     [(50, "50"), (70, "70"), (90, "90"), (110, "110"), (130, "130")], 3, 105),
    ("egt",   "VÝFUK", "°C ×100", 0, 1000,
     [(0, "0"), (200, "2"), (400, "4"), (600, "6"), (800, "8"), (1000, "10")], 3, 750),
    ("boost", "TURBO", "bar",       0, 2.5,
     [(0, "0"), (0.5, "0.5"), (1, "1"), (1.5, "1.5"), (2, "2"), (2.5, "2.5")], 4, 2.2),
    ("fuel",  "PALIVO", "\u00b0C",  0, 100,
     [(0, "0"), (20, "20"), (40, "40"), (60, "60"), (80, "80"), (100, "100")], 3, 80),
    ("dpf",   "DPF",   "g",         0, 40,
     [(0, "0"), (10, "10"), (20, "20"), (30, "30"), (40, "40")], 4, 24),
]


def ang(lo, hi, v):
    return math.radians(START + SWEEP * (v - lo) / (hi - lo))


def font(w, px):
    return ImageFont.truetype(FONT % w, int(px * SS))


def P(x):
    return int(round(x * SS))


SCALES = {}            # per dial: lo, hi, ticks, numerals -> a4_scales.h


def face(page):
    pid, name, unit, lo, hi, labels, minor, warn = page
    im = Image.new("RGB", (N * SS, N * SS), (0, 0, 0))
    d = ImageDraw.Draw(im)
    # black, a breath of warm grey towards the rim, as lit from the ring
    for r in range(232, 0, -3):
        k = max(0.0, (r - 140) / 92) ** 2
        g = int(2 + 10 * k)
        d.ellipse([P(C - r), P(C - r), P(C + r), P(C + r)], fill=(g + 3, g, g))

    # the red rim and its glow (light adds up)
    glow = Image.new("RGB", im.size, (0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse([P(C - R_RIM), P(C - R_RIM), P(C + R_RIM), P(C + R_RIM)],
               outline=(170, 14, 22), width=P(8))
    glow = glow.filter(ImageFilter.GaussianBlur(P(6)))
    im = ImageChops.add(im, glow)
    d = ImageDraw.Draw(im)
    d.ellipse([P(C - R_RIM), P(C - R_RIM), P(C + R_RIM), P(C + R_RIM)],
              outline=(235, 40, 44), width=P(2))

    if pid == "dpf":                    # no scale: icon and values, live
        return im.resize((N, N), Image.LANCZOS)

    # ticks: short dashes, the labelled ones a little longer and wider
    vals = [v for v, _ in labels]
    ticks = []
    for i, v in enumerate(vals):
        ticks.append((v, True))
        if i + 1 < len(vals):
            for k in range(1, minor + 1):
                ticks.append((v + (vals[i + 1] - v) * k / (minor + 1), False))

    # white dashes for the whole scale; past the warn limit (a setting)
    # ui_a4.c draws them over in red, with the numerals and the band
    marks = Image.new("RGB", im.size, (0, 0, 0))
    md = ImageDraw.Draw(marks)
    sc_ticks = []
    for v, major in ticks:
        a = ang(lo, hi, v)
        ln, w = (20, 7) if major else (12, 4.5)
        p0 = (C + (R_TICK - ln) * math.cos(a), C + (R_TICK - ln) * math.sin(a))
        p1 = (C + R_TICK * math.cos(a), C + R_TICK * math.sin(a))
        md.line([(P(p0[0]), P(p0[1])), (P(p1[0]), P(p1[1]))], fill=WHITE, width=P(w))
        sc_ticks.append((v, p0, p1, w))
    # lit print: a soft halo under the sharp marks
    halo = marks.filter(ImageFilter.GaussianBlur(P(3)))
    halo = Image.eval(halo, lambda c: int(c * 0.45))
    im = ImageChops.add(ImageChops.add(im, halo), marks)
    d = ImageDraw.Draw(im)

    # where the numerals go (drawn live, white or red): as close to the
    # ticks as each text's own size allows
    fnum = font(700, 52)
    sc_nums = []
    for v, t in labels:
        a = ang(lo, hi, v)
        l, t_, r_, b_ = md.textbbox((0, 0), t, font=fnum, anchor="mm")
        ext = (abs(math.cos(a)) * (r_ - l) / 2 + abs(math.sin(a)) * (b_ - t_) / 2) / SS
        rr = min(R_NUM, R_TICK - 26 - ext)
        sc_nums.append((v, t, C + rr * math.cos(a), C + rr * math.sin(a)))
    SCALES[pid] = (lo, hi, sc_ticks, sc_nums)

    # name and unit, lower right (the value goes above them, live)
    tx, ty = C + 104, C + 122
    d.text((P(tx), P(ty)), name, font=font(600, 26), fill=WHITE, anchor="mm")
    d.text((P(tx), P(ty + 28)), unit, font=font(500, 24), fill=GREY, anchor="mm")
    return im.resize((N, N), Image.LANCZOS)


def needle():
    """red and lit, pointing right (+x): from inside the hub out to
    the ticks, thin at the tip; the pivot is the image's left edge,
    middle. Size and pivot also in ui_a4.c."""
    L, W = 206, 30
    w, h = P(L), P(W)
    cy = h / 2
    body = [(P(30), cy - P(4)), (w - P(3), cy - P(1.5)), (w, cy),
            (w - P(3), cy + P(1.5)), (P(30), cy + P(4))]
    glow = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(glow).polygon(body, fill=(255, 20, 16, 190))
    glow = glow.filter(ImageFilter.GaussianBlur(P(4)))
    im = Image.alpha_composite(Image.new("RGBA", (w, h), (0, 0, 0, 0)), glow)
    d = ImageDraw.Draw(im)
    d.polygon(body, fill=(240, 30, 28, 255))
    d.line([(P(32), cy), (w - P(8), cy)], fill=(255, 140, 130, 255), width=P(1))
    im = im.resize((L, W), Image.LANCZOS)
    return im, 0, W // 2


def cap():
    """the hub: a big black disc with a faint dark red edge"""
    R = 34
    n = P(2 * R + 8)
    c = n / 2
    ring = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    ImageDraw.Draw(ring).ellipse([c - P(R), c - P(R), c + P(R), c + P(R)],
                                 outline=(220, 30, 30, 255), width=P(4))
    im = ring.filter(ImageFilter.GaussianBlur(P(3)))
    im = Image.alpha_composite(im, ring.filter(ImageFilter.GaussianBlur(P(1))))
    d = ImageDraw.Draw(im)
    d.ellipse([c - P(R - 1), c - P(R - 1), c + P(R - 1), c + P(R - 1)], fill=(6, 6, 7, 255))
    return im.resize((2 * R + 8, 2 * R + 8), Image.LANCZOS)


def regen_lamp():
    """the DPF tell-tale, amber: a filter body with soot dots, as on the
    cluster's lamp (drawn here, no maker's artwork)"""
    Wd, Hd = 60, 36
    im = Image.new("RGBA", (P(Wd), P(Hd)), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    s = SS
    amber = (255, 168, 0, 255)
    d.rounded_rectangle([10 * s, 6 * s, 50 * s, 30 * s], radius=5 * s, outline=amber, width=3 * s)
    d.line([(2 * s, 18 * s), (10 * s, 18 * s)], fill=amber, width=3 * s)
    d.line([(50 * s, 18 * s), (58 * s, 18 * s)], fill=amber, width=3 * s)
    for i, x in enumerate(range(17, 45, 7)):
        for y in (13, 23):
            yy = y + (2 if i % 2 else -2)
            d.ellipse([(x - 2) * s, (yy - 2) * s, (x + 2) * s, (yy + 2) * s], fill=amber)
    return im.resize((Wd, Hd), Image.LANCZOS)


def dpf_icon():
    """the DPF page's big icon, white (LVGL recolours it): the filter body
    with its pipes and soot dots, as the lamp, larger"""
    Wd, Hd = 170, 96
    s = SS
    im = Image.new("RGBA", (P(Wd), P(Hd)), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    wh = (255, 255, 255, 255)
    d.rounded_rectangle([28 * s, 12 * s, 142 * s, 84 * s], radius=14 * s,
                        outline=wh, width=7 * s)
    d.line([(2 * s, 48 * s), (28 * s, 48 * s)], fill=wh, width=7 * s)
    d.line([(142 * s, 48 * s), (168 * s, 48 * s)], fill=wh, width=7 * s)
    for i, x in enumerate(range(48, 126, 18)):
        for y in (34, 62):
            yy = y + (5 if i % 2 else -5)
            d.ellipse([(x - 6) * s, (yy - 6) * s, (x + 6) * s, (yy + 6) * s], fill=wh)
    return im.resize((Wd, Hd), Image.LANCZOS)


def c_rgb565(name, im):
    px = im.load()
    out = bytearray()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b = px[x, y][:3]
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            out += bytes((v & 0xFF, v >> 8))
    return c_img(name, out, im.width, im.height, "LV_IMG_CF_TRUE_COLOR")


def c_rgb565a(name, im):
    px = im.load()
    out = bytearray()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            out += bytes((v & 0xFF, v >> 8, a))
    return c_img(name, out, im.width, im.height, "LV_IMG_CF_TRUE_COLOR_ALPHA")


def c_img(name, data, w, h, cf):
    s = ["static const uint8_t %s_px[%d] = {" % (name, len(data))]
    for i in range(0, len(data), 24):
        s.append("    " + ",".join("0x%02X" % b for b in data[i:i + 24]) + ",")
    s.append("};\n")
    s.append("const lv_img_dsc_t %s = {\n    .header.cf = %s,\n"
             "    .header.w = %d, .header.h = %d,\n"
             "    .data_size = sizeof %s_px, .data = %s_px,\n};\n"
             % (name, cf, w, h, name, name))
    return "\n".join(s)


def write(fname, body):
    with open(os.path.join(ROOT, "main", "faces", fname), "w") as f:
        f.write("/* made by tools/gen_faces.py */\n#include \"lvgl.h\"\n\n" + body)


os.makedirs(os.path.join(ROOT, "main", "faces"), exist_ok=True)
os.makedirs(os.path.join(ROOT, "preview"), exist_ok=True)
for p in PAGES:
    im = face(p)
    im.save(os.path.join(ROOT, "preview", "face-%s.png" % p[0]))
    write("a4_face_%s.c" % p[0], c_rgb565("a4_face_%s" % p[0], im))
def lit(x):
    t = "%.6g" % x
    return t + ("f" if ("." in t or "e" in t) else ".0f")


h = ["/* made by tools/gen_faces.py: the dials' scales, for ui_a4.c */",
     "#pragma once", "",
     "typedef struct { float v; float x0, y0, x1, y1, w; } a4_tick_t;",
     "typedef struct { float v; const char *t; float x, y; } a4_num_t;",
     "typedef struct { float lo, hi; int n_ticks, n_nums;",
     "                 const a4_tick_t *ticks; const a4_num_t *nums; } a4_scale_t;",
     "",
     "#define A4_START_DEG %.1ff" % START, "#define A4_SWEEP_DEG %.1ff" % SWEEP,
     "#define A4_R_TICK %d" % R_TICK,
     "#define A4_R_BAND0 %d" % R_BAND[0], "#define A4_R_BAND1 %d" % R_BAND[1], ""]
for pid, (lo, hi, tk, nm) in SCALES.items():
    h.append("static const a4_tick_t A4_TICKS_%s[] = {" % pid.upper())
    for v, p0, p1, w in tk:
        h.append("    { %s, %.1ff, %.1ff, %.1ff, %.1ff, %.1ff }," % (lit(v), p0[0], p0[1], p1[0], p1[1], w))
    h.append("};")
    h.append("static const a4_num_t A4_NUMS_%s[] = {" % pid.upper())
    for v, t, x, y in nm:
        h.append('    { %s, "%s", %.1ff, %.1ff },' % (lit(v), t, x, y))
    h.append("};")
    h.append("#define A4_SCALE_%s { %s, %s, %d, %d, A4_TICKS_%s, A4_NUMS_%s }"
             % (pid.upper(), lit(lo), lit(hi), len(tk), len(nm), pid.upper(), pid.upper()))
    h.append("")
with open(os.path.join(ROOT, "main", "faces", "a4_scales.h"), "w") as f:
    f.write("\n".join(h))

nd, piv_x, piv_y = needle()
cp = cap()
lamp = regen_lamp()
write("a4_parts.c", c_rgb565a("a4_needle", nd) + "\n" + c_rgb565a("a4_cap", cp)
      + "\n" + c_rgb565a("a4_regen", lamp) + "\n" + c_rgb565a("a4_dpf_icon", dpf_icon()))
print("faces written; needle %dx%d pivot %d,%d; cap %d" % (nd.width, nd.height,
      piv_x, piv_y, cp.width))
