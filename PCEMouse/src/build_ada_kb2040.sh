#!/bin/sh
#
# until the next version of SDK comes out (when the KB2040 board definition is published),
# place the file "adafruit_kb2040.h" into (PICO_SDK)/src/boards/include/boards/
# (it will be part of the next release of the SDK)
#
cmake -DFAMILY=rp2040 -DPICO_BOARD=adafruit_kb2040 -B build
