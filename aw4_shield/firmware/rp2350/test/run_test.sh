#!/bin/sh
# Host test of aw4_rp2350 (logic + ST7789 framebuffer emulator); writes screenshots s*.ppm
set -e
cd "$(dirname "$0")"
g++ -std=gnu++17 -Wall -Wextra -Wno-unused-parameter -Imock -I../aw4_rp2350 -o test test.cpp
./test
