"""The NOT STOCK boot logo for the 2" gauge: ../notstock-round/assets/splash.png
at 200 x 200 on black, RGB565 LVGL 8 image -> main/l2_splash.c."""
import os
from PIL import Image

N = 200
ROOT = os.path.join(os.path.dirname(__file__), "..")
src = Image.open(os.path.join(ROOT, "..", "notstock-round", "assets", "splash.png")).convert("RGBA")
src = src.resize((N, N), Image.LANCZOS)
# the logo is a disc on white: keep the disc, black around it
im = Image.new("RGB", (N, N), (0, 0, 0))
mask = Image.new("L", (N, N), 0)
from PIL import ImageDraw
ImageDraw.Draw(mask).ellipse((N * 0.04, N * 0.04, N * 0.96, N * 0.96), fill=255)
im.paste(src.convert("RGB"), (0, 0), mask)
px = im.load()
out = bytearray()
for y in range(N):
    for x in range(N):
        r, g, b = px[x, y]
        v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        out += bytes((v & 0xFF, v >> 8))
with open(os.path.join(ROOT, "main", "l2_splash.c"), "w") as f:
    f.write("/* made by tools/gen_splash.py */\n#include \"lvgl.h\"\n\n")
    f.write("static const uint8_t px[%d] = {\n" % len(out))
    for i in range(0, len(out), 24):
        f.write("    " + ",".join("0x%02X" % b for b in out[i:i + 24]) + ",\n")
    f.write("};\n\nconst lv_img_dsc_t l2_splash = {\n"
            "    .header.cf = LV_IMG_CF_TRUE_COLOR,\n"
            "    .header.w = %d, .header.h = %d,\n"
            "    .data_size = sizeof px, .data = px,\n};\n" % (N, N))
im.save(os.path.join(ROOT, "preview", "splash.png"))
print("wrote main/l2_splash.c")
