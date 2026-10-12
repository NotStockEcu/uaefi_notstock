#!/usr/bin/env python3
"""The NOT STOCK boot logo for the 2.8" gauge: ../notstock-round/assets/
splash.png (a yellow disc on white) at 220 x 220 on black -> main/t5_splash.c.
The white around the disc goes by its colour, not by a cut circle: the
disc's yellow and black have no blue, the white is all blue, so the blue
channel is how much of a pixel is background; the edge stays smooth.
Run from the project root:  python3 tools/gen_splash28.py"""
import os
import numpy as np
from PIL import Image

N = 220
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
src = Image.open(os.path.join(ROOT, "..", "notstock-round", "assets",
                              "splash.png")).convert("RGB")
a = np.asarray(src).astype(np.float32) / 255
bg = a[:, :, 2:3]                       # 1: white background, 0: the disc
# take the white out of the mixed edge pixels, leave the disc's colours
out = np.clip(a - bg, 0, 1)
img = Image.fromarray((out * 255).round().astype(np.uint8))
img = img.crop(img.getbbox()).resize((N, N), Image.LANCZOS)
px = np.asarray(img).astype(np.uint16)
v = ((px[:, :, 0] >> 3) << 11) | ((px[:, :, 1] >> 2) << 5) | (px[:, :, 2] >> 3)
data = v.astype("<u2").tobytes()
with open(os.path.join(ROOT, "main", "t5_splash.c"), "w") as f:
    f.write("/* made by tools/gen_splash28.py */\n#include \"lvgl.h\"\n\n")
    f.write("static const uint8_t px[%d] = {\n" % len(data))
    for i in range(0, len(data), 24):
        f.write("    " + ",".join("0x%02X" % b for b in data[i:i + 24]) + ",\n")
    f.write("};\n\nconst lv_img_dsc_t t5_splash = {\n"
            "    .header.cf = LV_IMG_CF_TRUE_COLOR,\n"
            "    .header.w = %d, .header.h = %d,\n"
            "    .data_size = sizeof px, .data = px,\n};\n" % (N, N))
img.save(os.path.join(ROOT, "preview", "splash.png"))
print("wrote main/t5_splash.c")
