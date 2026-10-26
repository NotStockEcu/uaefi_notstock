#!/bin/sh
# Fonts of the 2.8" T5.1 gauge: the round gauge's (Orbitron, SIL OFL,
# ../notstock-round/assets/fonts), in the sizes of a 236 px dial. Orbitron
# gets the Czech C D E N R T U with caron / ring from the round gauge's
# tools/patch_font.py first.
# Needs lv_font_conv and fontTools. Run from the project root.
set -e
R=../notstock-round
F=$R/assets/fonts
O=main/fonts
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
PY=${PYTHON:-python3}
$PY $R/tools/patch_font.py $F/Orbitron-Bold.ttf $T/bold.ttf
CZ="ÁČĎÉĚÍŇÓŘŠŤÚŮÝŽ"
conv() {
    lv_font_conv --bpp 4 --format lvgl --no-compress --force-fast-kern-format \
        --lv-include lvgl.h "$@"
}
# the big dial's value: up to three digits, four (rpm, EGT), the DPF soot
conv --font $F/Orbitron-Black.ttf -r 0x2D-0x2E -r 0x30-0x39 --size 54 -o $O/t5_v54.c
conv --font $F/Orbitron-Black.ttf -r 0x2D-0x2E -r 0x30-0x39 --size 40 -o $O/t5_v40.c
# the DPF screen's rows
conv --font $F/Orbitron-Black.ttf -r 0x2D-0x2E -r 0x30-0x39 --size 20 -o $O/t5_v20.c
# the small dials' values
conv --font $F/Orbitron-Black.ttf -r 0x2D-0x2E -r 0x30-0x39 --size 16 -o $O/t5_v16.c
# trouble codes: P0401, U0100, P242F
conv --font $F/Orbitron-Black.ttf -r 0x30-0x39 --symbols ABCDEFPU --size 36 -o $O/t5_code_36.c
# menus, units, buttons
conv --font $T/bold.ttf -r 0x20-0x7E -r 0xB0 --symbols "$CZ" --size 14 -o $O/t5_t14.c
# titles, labels, the peak line
conv --font $T/bold.ttf -r 0x20-0x7E -r 0xB0 --symbols "$CZ" --size 11 -o $O/t5_t11.c
