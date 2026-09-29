#!/usr/bin/env python3
"""Render the round gauge (tools/sim) into preview/*.png plus a contact
sheet. Needs Pillow and LVGL 8.4:  LVGL_DIR=/path/to/lvgl python tools/preview.py
Extra args go to one custom render:  python tools/preview.py page=2 boost=1.8
"""
import os
import subprocess
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SIM = os.path.join(HERE, "sim")
OUT = os.path.join(ROOT, "preview")

SCENES = {
    "water": "page=0",
    "oil": "page=1",
    "boost": "page=2",
    "intake": "page=3",
    "exhaust": "page=4",
    "rpm": "page=5",
    "oil-warn": "page=1 oil=134",
    "no-data": "link=0",
    "swipe": "swipe=left t=0.2",
}


def render(name, args):
    ppm = os.path.join(SIM, "build", name + ".ppm")
    subprocess.run([os.path.join(SIM, "build", "sim"), ppm] + args.split(),
                   check=True)
    png = os.path.join(OUT, name + ".png")
    Image.open(ppm).save(png)
    os.remove(ppm)
    return png


def main():
    make = ["make", "-s", "-j8"]
    if "LVGL_DIR" in os.environ:
        make.append("LVGL_DIR=" + os.environ["LVGL_DIR"])
    subprocess.run(make, cwd=SIM, check=True)
    os.makedirs(OUT, exist_ok=True)
    if len(sys.argv) > 1:
        print(render("custom", " ".join(sys.argv[1:])))
        return
    pngs = [render(n, a) for n, a in SCENES.items()]
    n = Image.open(pngs[0]).size[0]
    sheet = Image.new("RGB", (n * 3, n * 3), (42, 42, 42))
    for i, p in enumerate(pngs):
        sheet.paste(Image.open(p), ((i % 3) * n, (i // 3) * n))
    sheet.save(os.path.join(OUT, "sheet.png"))
    print("preview/*.png, preview/sheet.png")


if __name__ == "__main__":
    main()
