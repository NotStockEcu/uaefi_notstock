"""The RED look's dials (ui_look_red.c): the A4 gauge's dials
(../notstock-a4/tools/gen_faces.py, after the A4 B8 cluster at night) for
the round gauge's pages and scales. A black face inside a thin red lit rim,
short white dashes for the scale, from just right of the bottom over the
left and the top round to 3 o'clock. Live in ui_look_red.c: the numerals
(white, red from the limit), the red ticks and band past the limit, the
needle and hub (images made here), name, unit and value. No maker's logo.

Writes main/faces480/red.c + red.h and main/faces466/red.c + red.h, and
preview/red-*.png. Run from the project root: python3 tools/gen_faces_red.py
"""
import math
import os
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.join(os.path.dirname(__file__), "..")
FONT = os.path.join(ROOT, "..", "notstock-a4", "assets", "fonts", "Barlow-%d-latin.ttf")
SS = 3
START, SWEEP = 95.0, 265.0          # degrees, clockwise from +x (screen)
WHITE = (238, 238, 236)

# round page order (ui_round.h: WATER, OIL, BOOST, INTAKE, EXHAUST, RPM);
# lo/hi as FACE_PAGE in faces.h. labels (value, text), minor ticks per gap
PAGES = [
    ("water",   40, 140, [(40, "40"), (60, "60"), (80, "80"), (100, "100"), (120, "120"), (140, "140")], 3),
    ("oil",     40, 160, [(40, "40"), (70, "70"), (100, "100"), (130, "130"), (160, "160")], 5),
    ("boost",   0, 2.0, [(0, "0"), (0.5, "0.5"), (1, "1"), (1.5, "1.5"), (2, "2")], 4),
    ("intake",  0, 100, [(0, "0"), (20, "20"), (40, "40"), (60, "60"), (80, "80"), (100, "100")], 3),
    ("exhaust", 0, 1000, [(0, "0"), (200, "2"), (400, "4"), (600, "6"), (800, "8"), (1000, "10")], 3),
    ("rpm",     0, 5000, [(0, "0"), (1000, "1"), (2000, "2"), (3000, "3"), (4000, "4"), (5000, "5")], 4),
]


def font(w, px):
    return ImageFont.truetype(FONT % w, int(px * SS))


def build(N):
    k = N / 466.0                     # the A4's geometry was drawn at 466
    C = N / 2
    R_RIM, R_TICK, R_NUM = 225 * k, 214 * k, 172 * k
    R_BAND = (190 * k, 197 * k)

    def P(x):
        return int(round(x * SS))

    def ang(lo, hi, v):
        return math.radians(START + SWEEP * (v - lo) / (hi - lo))

    faces, scales = [], []
    for pid, lo, hi, labels, minor in PAGES:
        im = Image.new("RGB", (N * SS, N * SS), (0, 0, 0))
        d = ImageDraw.Draw(im)
        for r in range(int(232 * k), 0, -3):
            q = max(0.0, (r - 140 * k) / (92 * k)) ** 2
            g = int(2 + 10 * q)
            d.ellipse([P(C - r), P(C - r), P(C + r), P(C + r)], fill=(g + 3, g, g))
        glow = Image.new("RGB", im.size, (0, 0, 0))
        ImageDraw.Draw(glow).ellipse([P(C - R_RIM), P(C - R_RIM), P(C + R_RIM), P(C + R_RIM)],
                                     outline=(170, 14, 22), width=P(8 * k))
        im = ImageChops.add(im, glow.filter(ImageFilter.GaussianBlur(P(6 * k))))
        ImageDraw.Draw(im).ellipse([P(C - R_RIM), P(C - R_RIM), P(C + R_RIM), P(C + R_RIM)],
                                   outline=(235, 40, 44), width=P(2))

        vals = [v for v, _ in labels]
        ticks = []
        for i, v in enumerate(vals):
            ticks.append((v, True))
            if i + 1 < len(vals):
                for j in range(1, minor + 1):
                    ticks.append((v + (vals[i + 1] - v) * j / (minor + 1), False))
        marks = Image.new("RGB", im.size, (0, 0, 0))
        md = ImageDraw.Draw(marks)
        sc_ticks = []
        for v, major in ticks:
            a = ang(lo, hi, v)
            ln, w = (20 * k, 7) if major else (12 * k, 4.5)
            p0 = (C + (R_TICK - ln) * math.cos(a), C + (R_TICK - ln) * math.sin(a))
            p1 = (C + R_TICK * math.cos(a), C + R_TICK * math.sin(a))
            md.line([(P(p0[0]), P(p0[1])), (P(p1[0]), P(p1[1]))], fill=WHITE, width=P(w))
            sc_ticks.append((v, p0, p1, w))
        halo = Image.eval(marks.filter(ImageFilter.GaussianBlur(P(3))), lambda c: int(c * 0.45))
        im = ImageChops.add(ImageChops.add(im, halo), marks)

        # numerals (live, in the 52 px font): as close to the ticks as each
        # text's own size allows
        fnum = font(700, 52)
        sc_nums = []
        for v, t in labels:
            a = ang(lo, hi, v)
            l, t_, r_, b_ = md.textbbox((0, 0), t, font=fnum, anchor="mm")
            ext = (abs(math.cos(a)) * (r_ - l) / 2 + abs(math.sin(a)) * (b_ - t_) / 2) / SS
            rr = min(R_NUM, R_TICK - 26 * k - ext)
            sc_nums.append((v, t, C + rr * math.cos(a), C + rr * math.sin(a)))
        faces.append((pid, im.resize((N, N), Image.LANCZOS)))
        scales.append((pid, lo, hi, sc_ticks, sc_nums))

    # the needle: red and lit, pointing right, pivot at its left edge, middle
    L, W = int(round(206 * k)), 30
    w, h = P(L), P(W)
    cy = h / 2
    body = [(P(30 * k), cy - P(4)), (w - P(3), cy - P(1.5)), (w, cy),
            (w - P(3), cy + P(1.5)), (P(30 * k), cy + P(4))]
    glow = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(glow).polygon(body, fill=(255, 20, 16, 190))
    nd = Image.alpha_composite(Image.new("RGBA", (w, h), (0, 0, 0, 0)),
                               glow.filter(ImageFilter.GaussianBlur(P(4))))
    dd = ImageDraw.Draw(nd)
    dd.polygon(body, fill=(240, 30, 28, 255))
    dd.line([(P(32 * k), cy), (w - P(8), cy)], fill=(255, 140, 130, 255), width=P(1))
    nd = nd.resize((L, W), Image.LANCZOS)

    # the hub: a big black disc with a faint dark red edge
    R = int(round(34 * k))
    n = P(2 * R + 8)
    c = n / 2
    ring = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    ImageDraw.Draw(ring).ellipse([c - P(R), c - P(R), c + P(R), c + P(R)],
                                 outline=(220, 30, 30, 255), width=P(4))
    cp = Image.alpha_composite(ring.filter(ImageFilter.GaussianBlur(P(3))),
                               ring.filter(ImageFilter.GaussianBlur(P(1))))
    ImageDraw.Draw(cp).ellipse([c - P(R - 1), c - P(R - 1), c + P(R - 1), c + P(R - 1)],
                               fill=(6, 6, 7, 255))
    cp = cp.resize((2 * R + 8, 2 * R + 8), Image.LANCZOS)
    geo = dict(R_TICK=R_TICK, R_BAND0=R_BAND[0], R_BAND1=R_BAND[1], NEEDLE_PY=W // 2)
    return faces, scales, nd, cp, geo


def c_img(name, data, w, h, cf):
    s = ["static const uint8_t %s_px[%d] = {" % (name, len(data))]
    for i in range(0, len(data), 24):
        s.append("    " + ",".join("0x%02X" % b for b in data[i:i + 24]) + ",")
    s.append("};")
    s.append("const lv_img_dsc_t %s = {\n    .header.cf = %s,\n"
             "    .header.w = %d, .header.h = %d,\n"
             "    .data_size = sizeof %s_px, .data = %s_px,\n};\n" % (name, cf, w, h, name, name))
    return "\n".join(s)


def rgb565(name, im, alpha=False):
    px = im.load()
    out = bytearray()
    for y in range(im.height):
        for x in range(im.width):
            p = px[x, y]
            v = ((p[0] >> 3) << 11) | ((p[1] >> 2) << 5) | (p[2] >> 3)
            out += bytes((v & 0xFF, v >> 8)) + (bytes((p[3],)) if alpha else b"")
    return c_img(name, out, im.width, im.height,
                 "LV_IMG_CF_TRUE_COLOR_ALPHA" if alpha else "LV_IMG_CF_TRUE_COLOR")


def lit(x):
    t = "%.6g" % x
    return t + ("f" if ("." in t or "e" in t) else ".0f")


os.makedirs(os.path.join(ROOT, "preview"), exist_ok=True)
for N in (480, 466):
    faces, scales, nd, cp, geo = build(N)
    out = os.path.join(ROOT, "main", "faces%d" % N)
    body = ["/* made by tools/gen_faces_red.py: the RED look's dials, %d px */" % N,
            '#include "lvgl.h"', ""]
    for pid, im in faces:
        if N == 480:
            im.save(os.path.join(ROOT, "preview", "red-face-%s.png" % pid))
        body.append(rgb565("red_face_%s" % pid, im))
    body.append(rgb565("red_needle", nd, True))
    body.append(rgb565("red_cap", cp, True))
    body.append("const lv_img_dsc_t *const red_face[6] = {\n    %s,\n};\n"
                % ", ".join("&red_face_%s" % p for p, _ in faces))
    open(os.path.join(out, "red.c"), "w").write("\n".join(body))

    h = ["/* made by tools/gen_faces_red.py: the RED look's dials, %d px */" % N,
         "#pragma once", '#include "lvgl.h"', "",
         "typedef struct { float v; float x0, y0, x1, y1, w; } red_tick_t;",
         "typedef struct { float v; const char *t; float x, y; } red_num_t;",
         "typedef struct { float lo, hi; int n_ticks, n_nums;",
         "                 const red_tick_t *ticks; const red_num_t *nums; } red_scale_t;", "",
         "#define RED_START_DEG %.1ff" % START, "#define RED_SWEEP_DEG %.1ff" % SWEEP,
         "#define RED_R_BAND0 %d" % round(geo["R_BAND0"]), "#define RED_R_BAND1 %d" % round(geo["R_BAND1"]),
         "#define RED_NEEDLE_PY %d    /* the pivot: left edge, this far down */" % geo["NEEDLE_PY"], "",
         "extern const lv_img_dsc_t *const red_face[6];   /* in page order */",
         "extern const lv_img_dsc_t red_needle, red_cap;", ""]
    names = []
    for pid, lo, hi, tk, nm in scales:
        u = pid.upper()
        h.append("static const red_tick_t RED_TICKS_%s[] = {" % u)
        for v, p0, p1, w in tk:
            h.append("    { %s, %.1ff, %.1ff, %.1ff, %.1ff, %.1ff }," % (lit(v), p0[0], p0[1], p1[0], p1[1], w))
        h.append("};")
        h.append("static const red_num_t RED_NUMS_%s[] = {" % u)
        for v, t, x, y in nm:
            h.append('    { %s, "%s", %.1ff, %.1ff },' % (lit(v), t, x, y))
        h.append("};")
        names.append("{ %s, %s, %d, %d, RED_TICKS_%s, RED_NUMS_%s }" % (lit(lo), lit(hi), len(tk), len(nm), u, u))
    h.append("static const red_scale_t RED_SCALE[6] = {")
    for s in names:
        h.append("    %s," % s)
    h.append("};")
    open(os.path.join(out, "red.h"), "w").write("\n".join(h) + "\n")
    print("faces%d: needle %dx%d, cap %d" % (N, nd.width, nd.height, cp.width))
