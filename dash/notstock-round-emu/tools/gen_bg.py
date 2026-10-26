"""The page background: a dark honeycomb, darker to the rim, as on
ECUMaster's round gauges. Writes main/emu_bg.c (480 x 480 RGB565, LVGL 8
image) and preview/bg.png."""
import math
import os
from PIL import Image, ImageDraw

N = 480
ROOT = os.path.join(os.path.dirname(__file__), "..")
SS = 2                                   # supersampled, then shrunk
im = Image.new("RGB", (N * SS, N * SS), (14, 15, 17))
d = ImageDraw.Draw(im)
R = 17 * SS                              # hex radius (centre to corner)
w, h = math.sqrt(3) * R, 1.5 * R         # pointy-top rows
for row in range(-1, int(N * SS / h) + 2):
    for col in range(-1, int(N * SS / w) + 2):
        cx = col * w + (w / 2 if row % 2 else 0)
        cy = row * h
        pts = [(cx + R * math.cos(math.radians(60 * k + 30)),
                cy + R * math.sin(math.radians(60 * k + 30))) for k in range(6)]
        d.polygon(pts, outline=(40, 43, 47), width=2 * SS - 1)
im = im.resize((N, N), Image.LANCZOS)
# vignette: full brightness in the middle, a third at the rim
px = im.load()
for y in range(N):
    for x in range(N):
        r = math.hypot(x - N / 2 + 0.5, y - N / 2 + 0.5) / (N / 2)
        k = 1.0 if r < 0.55 else max(0.35, 1.0 - (r - 0.55) * 1.4)
        c = px[x, y]
        px[x, y] = tuple(int(v * k) for v in c)
os.makedirs(os.path.join(ROOT, "preview"), exist_ok=True)
im.save(os.path.join(ROOT, "preview", "bg.png"))

out = bytearray()
for y in range(N):
    for x in range(N):
        r, g, b = px[x, y]
        v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        out += bytes((v & 0xFF, v >> 8))
with open(os.path.join(ROOT, "main", "emu_bg.c"), "w") as f:
    f.write("/* made by tools/gen_bg.py */\n#include \"lvgl.h\"\n\n")
    f.write("static const uint8_t px[%d] = {\n" % len(out))
    for i in range(0, len(out), 24):
        f.write("    " + ",".join("0x%02X" % b for b in out[i:i + 24]) + ",\n")
    f.write("};\n\nconst lv_img_dsc_t emu_bg = {\n"
            "    .header.cf = LV_IMG_CF_TRUE_COLOR,\n"
            "    .header.w = %d, .header.h = %d,\n"
            "    .data_size = sizeof px, .data = px,\n};\n" % (N, N))
print("wrote main/emu_bg.c")
