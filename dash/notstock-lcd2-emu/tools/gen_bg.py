"""The page background of the 2" EMU gauge: the dark honeycomb of the round
EMU gauge, darker to the edges, and a NOT STOCK hazard stripe (yellow and
black) along the bottom. Writes main/l2_bg.c (240 x 320 RGB565, LVGL 8
image) and preview/bg.png."""
import math
import os
from PIL import Image, ImageDraw

W, H = 240, 320
STRIPE = 6                               # hazard stripe at the bottom, px
YEL = (255, 213, 0)
ROOT = os.path.join(os.path.dirname(__file__), "..")
SS = 3                                   # supersampled, then shrunk
im = Image.new("RGB", (W * SS, H * SS), (14, 15, 17))
d = ImageDraw.Draw(im)
R = 11 * SS                              # hex radius (centre to corner)
w, h = math.sqrt(3) * R, 1.5 * R         # pointy-top rows
for row in range(-1, int(H * SS / h) + 2):
    for col in range(-1, int(W * SS / w) + 2):
        cx = col * w + (w / 2 if row % 2 else 0)
        cy = row * h
        pts = [(cx + R * math.cos(math.radians(60 * k + 30)),
                cy + R * math.sin(math.radians(60 * k + 30))) for k in range(6)]
        d.polygon(pts, outline=(40, 43, 47), width=2 * SS - 2)
# the stripe: 45 degree bands, yellow and black
y0 = (H - STRIPE) * SS
d.rectangle((0, y0, W * SS, H * SS), fill=(0, 0, 0))
band = 8 * SS
for x in range(-H * SS, W * SS + band * 2, band * 2):
    d.polygon([(x, H * SS), (x + band, H * SS), (x + band + STRIPE * SS, y0),
               (x + STRIPE * SS, y0)], fill=YEL)
im = im.resize((W, H), Image.LANCZOS)
# vignette on the honeycomb only: full in the middle, a third at the edges
px = im.load()
for y in range(H - STRIPE):
    for x in range(W):
        r = max(abs(x - W / 2 + 0.5) / (W / 2), abs(y - H / 2 + 0.5) / (H / 2))
        k = 1.0 if r < 0.5 else max(0.35, 1.0 - (r - 0.5) * 1.3)
        c = px[x, y]
        px[x, y] = tuple(int(v * k) for v in c)
os.makedirs(os.path.join(ROOT, "preview"), exist_ok=True)
im.save(os.path.join(ROOT, "preview", "bg.png"))

out = bytearray()
for y in range(H):
    for x in range(W):
        r, g, b = px[x, y]
        v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        out += bytes((v & 0xFF, v >> 8))
with open(os.path.join(ROOT, "main", "l2_bg.c"), "w") as f:
    f.write("/* made by tools/gen_bg.py */\n#include \"lvgl.h\"\n\n")
    f.write("static const uint8_t px[%d] = {\n" % len(out))
    for i in range(0, len(out), 24):
        f.write("    " + ",".join("0x%02X" % b for b in out[i:i + 24]) + ",\n")
    f.write("};\n\nconst lv_img_dsc_t l2_bg = {\n"
            "    .header.cf = LV_IMG_CF_TRUE_COLOR,\n"
            "    .header.w = %d, .header.h = %d,\n"
            "    .data_size = sizeof px, .data = px,\n};\n" % (W, H))
print("wrote main/l2_bg.c")
