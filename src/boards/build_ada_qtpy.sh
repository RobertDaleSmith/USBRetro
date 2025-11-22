#!/bin/sh
cmake -G "Unix Makefiles" -DFAMILY=rp2040 -DPICO_BOARD=adafruit_qtpy_rp2040 -B build
