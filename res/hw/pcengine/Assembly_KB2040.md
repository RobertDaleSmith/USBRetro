# PC_Engine_RP2040_Projects - PC Engine Mouse converter (KB2040 version)

## Overview

Please take a look at the main PCEMouse/Readme.md file for the overview; this
file is specific to the KB2040 revision.


## PC Board and Assembly

I designed all boards using the free version of EAGLE (2-layer, less than 100mm on both X- and Y- axes).
The gerbers are included in this repository, in case you want to get your own set made.

I have included the gerbers and relevant bom.csv and assembly.csv files to get these boards
assembled by JLCPCB, but you will still need the following parts:
- (1) Adafruit KB2040 microcontroller board
- (2) headers.  You may also want to use female pin header sockets in case you need to remove the microcontroller board.
I recommend using short/low-profile header sockets in this case.

... I had thought to make these boards solderable along the castellation, but the pads below may not be wide enough
to accommodate this.  I will address this in a future revision.

Steps to assembly:
1. Trim the leads of the through-hole parts carefully, to minimize the solder "bump" on the underside of the board when mounted.
The through-hole parts to be mounted include the 8-pin mini-DIN socket, and the headers (or sockets) for the RP2040 board.
2. Solder carefully, minimizing the amount of "bump" below the board
2. Note that the KB2040 board should be soldered to the **UNDERSIDE** of the main board, as shown in the pictures:
<img src="https://github.com/dshadoff/PC_Engine_RP2040_Projects/blob/main/img/PCEMouse_KB2040a.JPG" width="378" height="504">
<img src="https://github.com/dshadoff/PC_Engine_RP2040_Projects/blob/main/img/PCEMouse_KB2040b.JPG" width="378" height="504">
<img src="https://github.com/dshadoff/PC_Engine_RP2040_Projects/blob/main/img/PCEMouse_KB2040c.JPG" width="378" height="504">
3. Mount the RP2040 board with the USB connector facing away from the 8-pin DIN connector.
4. Connect it to a host computer while holding the "Boot" button down.  This will put it in DFU mode and create a virtual drive
on the host computer.  Drag and drop the *.uf2 firmware file into that folder.  A moment later, the virtual drive should disappear
and after about 1 second, all four LEDs should light up briefly as part of the boot sequence of the memory device.  Disconnect it from USB.


## Source Code

### Compilation

This was updated to use Pico-SDK version 1.4.0, which may not be the newest version when you read this.
The special header file for the KB2040 board is no longer needed (as of SDK 1.3.1).

pico_sdk_import.cmake is from the SDK, but is required by CMake (and thus replicated here)

This is based on the TinyUSB Host HID example, and since this often changes (required by refactoring of
the Host HID code), the initial commit baseline is the source ocde of that example.

To build the source, first ensure that you have the right version of the RaspberryPi/piso-sdk installed.
As this board targets the Adafruit QtPy2040 board, you should run the make_ada.sh script (under UNIX).
Then, "cd build" and "make".

I have also included a release version of the program as a uf2 file in the releases/ folder; just drag and drop it
onto the virtual drive presented when putting the board into BOOTSEL mode (holding the 'boot' button, connect the
board by USB to a host computer, and release the button; a new drive should appear on the computer).

