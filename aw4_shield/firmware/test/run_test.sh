#!/bin/sh
# Host test of the firmware logic (no Arduino needed): mocks digitalRead/Write, millis and SoftwareSerial.
set -e
cd "$(dirname "$0")"
sed -e 's/#include <SoftwareSerial.h>/#include "mock.h"/' ../aw4_controller.ino > fw.cpp
g++ -std=c++11 -Wall -o test test.cpp
./test
