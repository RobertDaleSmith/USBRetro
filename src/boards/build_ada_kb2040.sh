#!/bin/sh
cmake -G "Unix Makefiles" -DFAMILY=rp2040 -DPICO_BOARD=adafruit_kb2040 -B build
