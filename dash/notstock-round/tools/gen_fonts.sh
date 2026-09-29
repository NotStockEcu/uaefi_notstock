#!/bin/sh
# Orbitron fonts for the round gauge. Source: assets/fonts/Orbitron-*.ttf,
# static instances of Google Fonts' Orbitron[wght].ttf (SIL OFL, see
# assets/fonts/OFL-Orbitron.txt).
#
# Needs lv_font_conv:  npm i -g lv_font_conv
# Run from the project root:  sh tools/gen_fonts.sh
set -e
F=assets/fonts
O=main/fonts
conv() {
    lv_font_conv --bpp 4 --format lvgl --no-compress --force-fast-kern-format \
        --lv-include lvgl.h "$@"
}
# the value in the middle
conv --font $F/Orbitron-Black.ttf -r 0x2D-0x2E -r 0x30-0x39 --size 112 -o $O/rnd_112.c
# four-digit values (rpm, exhaust)
conv --font $F/Orbitron-Black.ttf -r 0x2D-0x2E -r 0x30-0x39 --size 84 -o $O/rnd_84.c
# title and unit: WATER, OIL, degC, bar, rpm
conv --font $F/Orbitron-Bold.ttf -r 0x20-0x7E -r 0xB0 --size 26 -o $O/rnd_26.c
# scale numbers, peak line
conv --font $F/Orbitron-Bold.ttf -r 0x20-0x7E -r 0xB0 --size 18 -o $O/rnd_18.c
