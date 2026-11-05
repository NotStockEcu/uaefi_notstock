#!/bin/bash
# build.sh <sketch_dir> <out_dir>  – compile an Arduino sketch for RP2350 (Pico 2 FQBN) with the apt toolchain
cd /opt/rp && ./arduino-cli compile --fqbn pico:rp2040:rpipico2 --warnings all --output-dir "$2" \
  --build-property "runtime.tools.pqt-gcc.path=/usr" \
  --build-property "runtime.tools.pqt-python3.path=/usr/bin" \
  --build-property "runtime.tools.pqt-picotool.path=/opt/rp/tools" \
  --build-property "runtime.tools.pqt-openocd.path=/opt/rp/tools" \
  --build-property "runtime.tools.ctags.path=/opt/rp/tools/ctags" \
  --build-property "build.extra_flags=-I/opt/rp/fakeinc/x $EXTRA" \
  --build-property "compiler.c.flags=-c {compiler.warning_flags} {compiler.defines} {compiler.flags} -MMD {compiler.includes} -std=gnu2x -g -pipe" \
  --build-property "recipe.objcopy.uf2.pattern=/usr/bin/python3 /home/user/uaefi_notstock/aw4_shield/tools/rp2350/elf2uf2.py {build.path}/{build.project_name}.elf {build.path}/{build.project_name}.uf2" \
  "$1" 2>&1 | grep -v "^Error initializing\|Downloading index"
