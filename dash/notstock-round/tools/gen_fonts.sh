#!/bin/sh
# Fonts for the round gauge. Sources: assets/fonts/Orbitron-*.ttf, static
# instances of Google Fonts' Orbitron[wght].ttf, and Barlow Condensed (both
# SIL OFL, see assets/fonts/OFL-*.txt).
#
# Orbitron has no C D E N R T U with caron / ring, which Czech needs:
# tools/patch_font.py adds them to a temporary, renamed copy first.
#
# Needs lv_font_conv (npm i -g lv_font_conv) and fontTools (pip install
# fonttools). Run from the project root:  sh tools/gen_fonts.sh
set -e
F=assets/fonts
O=main/fonts
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
PY=${PYTHON:-python3}
$PY tools/patch_font.py $F/Orbitron-Bold.ttf $T/bold.ttf

# text: ASCII, the degree sign, Czech capitals
CZ="ÁČĎÉĚÍŇÓŘŠŤÚŮÝŽ"
conv() {
    lv_font_conv --bpp 4 --format lvgl --no-compress --force-fast-kern-format \
        --lv-include lvgl.h "$@"
}
# the value in the middle
conv --font $F/Orbitron-Black.ttf -r 0x2D-0x2E -r 0x30-0x39 --size 112 -o $O/rnd_112.c
# four-digit values (rpm, exhaust)
conv --font $F/Orbitron-Black.ttf -r 0x2D-0x2E -r 0x30-0x39 --size 84 -o $O/rnd_84.c
# trouble codes on the DIAGNOSTICS screen: P0401, U0100, P242F
conv --font $F/Orbitron-Black.ttf -r 0x30-0x39 --symbols ABCDEFPU --size 56 -o $O/rnd_56.c
# menus, units, toasts
conv --font $T/bold.ttf -r 0x20-0x7E -r 0xB0 --symbols "$CZ" --size 26 -o $O/rnd_26.c
# page titles, scale numbers, peak line, hints
conv --font $T/bold.ttf -r 0x20-0x7E -r 0xB0 --symbols "$CZ" --size 18 -o $O/rnd_18.c
# RETRO look: the readout window, the page title, the unit (Barlow Condensed)
conv --font $F/BarlowCondensed-Bold.ttf -r 0x2D-0x2E -r 0x30-0x39 --size 46 -o $O/rnd_barlow_46.c
conv --font $F/BarlowCondensed-SemiBold.ttf -r 0x20-0x7E -r 0xB0 --symbols "$CZ" --size 23 -o $O/rnd_barlow_23.c
conv --font $F/BarlowCondensed-SemiBold.ttf -r 0x20-0x7E -r 0xB0 --symbols "$CZ" --size 20 -o $O/rnd_barlow_20.c
