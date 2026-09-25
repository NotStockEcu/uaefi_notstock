#!/bin/sh
# Orbitron fonts for the speed readout, the rev counter readout and its labels.
# Source: assets/fonts/Orbitron-*.ttf, static instances of Google Fonts'
# Orbitron[wght].ttf (SIL OFL, see assets/fonts/OFL-Orbitron.txt).
# The DejaVu fonts in main/fonts/ carry their own lv_font_conv line in their
# headers.
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
# speed, digits only
conv --font $F/Orbitron-Black.ttf -r 0x30-0x39 --size 56 -o $O/dash_speed_56.c
# rpm readout inside the rev counter
conv --font $F/Orbitron-Bold.ttf -r 0x2D-0x2E -r 0x30-0x39 --size 40 -o $O/dash_orb_40.c
# small labels: km/h, x1000 rpm
conv --font $F/Orbitron-Bold.ttf -r 0x20-0x7E --size 18 -o $O/dash_orb_18.c
# side gauge readouts
conv --font $F/Orbitron-Bold.ttf -r 0x2D-0x2E -r 0x30-0x39 --size 30 -o $O/dash_orb_30.c
# side gauge units and titles: bar, degC, TURBO, AFR
conv --font $F/Orbitron-Bold.ttf -r 0x20-0x7E -r 0xB0 --size 14 -o $O/dash_orb_14.c
