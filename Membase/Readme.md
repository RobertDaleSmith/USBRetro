# PC_Engine_RP2040_Projects - Memory Base 128 reimplementation

## Overview

This is the third implementation of the Memory Base 128 I have written for modern hardware, and the easiest code to read/follow.
The PIOs on the Raspberry Pi Pico microcontroller are used for edge-sensing of the data input, and the ARM core is used for processing.

The data is loaded into SRAM at startup, and saved into Flash after transactions take place.  This may not be ideal, as the
device is not capable of processing additional commands while the Flash flush is in progress (it takes place 0.75 seconds
after the last read/write of a group of read or write transactions).  While this flush is taking place, the PIO state machine
is shut down, so there won't be a backlog of bits causing confusion; however, if any transactions take place during the period of
flash write, they won't be recognized, and the PC Engine game may identify an error.

## PC Board & Assembly

I designed all boards using the free version of EAGLE (2-layer, less than 100mm on both X- and Y- axes).
The gerbers are included in this repository, in case you want to get your own set made.

I have included the gerbers and relevant bom.csv and assembly.csv files to get these boards
assembled by JLCPCB, but you will still need the following parts:
- (2) 8-pin Mini-DIN connectors
- (1) Adafruit QtPy RP2040 microcontroller board
- (2) 7-pin headers.  You may also want to use female pin header sockets in case you need to remove the microcontroller board.
I recommend using short/low-profile header sockets in this case.


## Source Code

This was built using Pico-SDK version 1.2.0
pico_sdk_import.cmake is from the SDK, but is required by CMake (and thus replicated here)

To build the source, first ensure that you have the right version of the RaspberryPi/piso-sdk installed.
As this board targets the Adafruit QtPy2040 board, you should run the make_ada.sh script (under UNIX), or from
the command-line, "cmake -DPICO_BOARD=adafruit_qtpy_rp2040 -B build"
Then, "cd build" and "make".

I have also included a release version of the program as a uf2 file in the releases/ folder; just drag and drop it
onto the virtual drive presented when putting the board into BOOTSEL mode (holding the 'boot' button, connect the
board by USB to a host computer, and release the button; a new drive should appear on the computer).

## Notes
I have tried to use the Adafruit QtPy RP2040 as much as possible, as it is a compact form factor which is easy to design around.

Unfortunately, the Adafruit site tries to direct all users toward CircuitPython rather than the Pi SDK, and
as a result, Pinout pages of the "Pinout" page for their RP2040 devices don't include references to the GPIO numbers.

Therefore, I am including a graphic here:

![Adafruit QtPy RP2040 GPIO pinout](../img/qtpy_rp2040_GPIO.png)

