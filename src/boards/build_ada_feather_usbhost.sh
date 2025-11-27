#!/bin/sh
cmake -G "Unix Makefiles" -DFAMILY=rp2040 -DPICO_BOARD=adafruit_feather_rp2040_usb_host -B build
