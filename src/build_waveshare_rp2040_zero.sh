#!/bin/sh
cmake -G "Unix Makefiles" -DFAMILY=rp2040 -DPICO_BOARD=waveshare_rp2040_zero -B build
