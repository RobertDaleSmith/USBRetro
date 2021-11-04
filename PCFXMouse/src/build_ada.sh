#!/bin/sh
# cmake -DPICO_TINYUSB_PATH=/home/pi/devel/pico/pico-setup/pico/tinyusb -DFAMILY=rp2040 -DPICO_BOARD=adafruit_qtpy_rp2040 -B build
cmake -DFAMILY=rp2040 -DPICO_BOARD=adafruit_qtpy_rp2040 -B build
