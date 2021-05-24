# PC_Engine_RP2040_Projects - Memory Base 128 reimplementation

## Overview

This is the third implementation of the Memory Base 128 I have written for modern hardware, and the easiest code to read.
The PIOs areused for edge-sensing of the data input, and the ARM core is used for processing.

The data is loaded into SRAM at startup, and saved into Flash after transactions take place.  This may not be ideal, as the
device is not capable of processing additional commands while the Flash flush is in progress (it takes place 0.75 seconds
after the last read/write of a group of read or write transactions).

## Source Build

This was built using Pico-SDK version 1.1.2
pico_sdk_import.cmake is from the SDK, but is required by CMake (and thus replicated here)

The Adafruit QtPy_RP2040 has a minor incompatibility with this version of the SDK, requiring two adjustments:
- While the program will run fine when dropped directly into the device, it will not reset properly when the RESET button is pressed, or when power is cycled.  This appears to be an issue on some, but not all, QtPy RP2040 devices.  The fixes required are:
 - pico-sdk/src/boards/include/boards/adafruit_qtpy_rp2040.h : change #define PICO_FLASH_SPI_CLKDIV 2   to 4
 - pico-sdk/src/rp2_common/hardware_xosc/xosc.c : change xosc_hw->startup = startup_delay; to startup_delay * 32

## Notes
I have tried to use the Adafruit QtPy RP2040 as much as possible, as it is a compact form factor which is easy to design around.

Unfortunately, the Adafruit site tries todirect all users toward their verison of CircuitPython rather than the Pi SDK, and
as a result, Pinout pages of the "Pinout" page for their RP2040 devices don't include references to the GPIO numbers.

Therefore, I am including a graphic here:

![Adafruit QtPy RP2040 GPIO pinout](img/qtpy_rp2040_GPIO.png)

