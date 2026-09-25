#!/usr/bin/env python3
"""Render the dash on the PC, without flashing.

Builds tools/sim (the real main/ui.c, ui_menu.c, settings.c, fonts and
artwork against LVGL on the host) and renders a few scenes into preview/.
What you see is what the panel draws, pixel for pixel, because nothing here
re-implements the layout.

    python tools/preview.py                     # the standard scenes
    python tools/preview.py rpm=6500 boost=1.4  # one custom scene

Inputs: rpm speed clt iat boost afr, link=0 for the NO CAN state, demo=1 t=3.2
for the demo generator at time t, screen=menu for the settings screen,
boot=<ms> for the boot animation at that time after power-up.

Needs make, a C compiler, Pillow and LVGL 8.4. After one `idf.py build` LVGL
sits in managed_components/ and is found automatically; otherwise set
LVGL_DIR to any LVGL v8.4 checkout.
"""
import os
import subprocess
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
SIM = os.path.join(HERE, "sim")
OUT = os.path.join(ROOT, "preview")

SCENES = {
    "dash": "rpm=5700 speed=87 clt=92 iat=38 boost=0.85 afr=12.4",
    "dash-limits": "rpm=7400 speed=238 clt=118 iat=72 boost=1.45 afr=17.6",
    "dash-idle": "rpm=850 speed=0 clt=45 iat=21 boost=-0.65 afr=14.7",
    "dash-nocan": "link=0",
    "menu": "screen=menu",
    "boot-in": "boot=500 rpm=900 clt=70 iat=25 boost=-0.6 afr=14.7",
    "boot-cross": "boot=2350 rpm=900 clt=70 iat=25 boost=-0.6 afr=14.7",
}


def build():
    env = dict(os.environ)
    if "LVGL_DIR" not in env:
        mc = os.path.join(ROOT, "managed_components", "lvgl__lvgl")
        if not os.path.isdir(mc):
            sys.exit("LVGL not found. Run `idf.py build` once, or set LVGL_DIR "
                     "to an LVGL v8.4 checkout.")
        env["LVGL_DIR"] = mc
    subprocess.run(["make", "-s", "-j8", "LVGL_DIR=" + env["LVGL_DIR"]],
                   cwd=SIM, env=env, check=True)


def render(name, args):
    ppm = os.path.join(SIM, "build", name + ".ppm")
    subprocess.run([os.path.join(SIM, "build", "sim"), ppm] + args.split(),
                   check=True)
    png = os.path.join(OUT, name + ".png")
    Image.open(ppm).save(png)
    os.remove(ppm)
    print(os.path.relpath(png, ROOT))


def main():
    build()
    os.makedirs(OUT, exist_ok=True)
    if len(sys.argv) > 1:
        render("custom", " ".join(sys.argv[1:]))
    else:
        for name, args in SCENES.items():
            render(name, args)


if __name__ == "__main__":
    main()
