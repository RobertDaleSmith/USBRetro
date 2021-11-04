# PC_Engine_RP2040_Projects - PC-FX Mouse converter

## Overview

This project is intended to allow use of a standard USB mouse on a PC-FX.
While the code and board are in working condition, there is likely still room for improvement,
as the TinyUSB library and pico-sdk are still evolving, and the board could be improved if rebuilt from
basic parts, and using a USB-A connector.


## PC Board and Assembly

I designed all boards using the free version of EAGLE (2-layer, less than 100mm on both X- and Y- axes).
The gerbers are included in this repository, in case you want to get your own set made.

I have included the gerbers and relevant bom.csv and assembly.csv files to get these boards
assembled by JLCPCB, but you will still need the following parts:
- (1) Adafruit QtPy RP2040 microcontroller board
- (7) pieces of Mill-Max pin&socket connector, Part number 4401-0-15-80-18-27-04-0
- (2) 7-pin headers.  You may also want to use female pin header sockets in case you need to remove the microcontroller board.
I recommend using short/low-profile header sockets in this case.

There are two PC Boards in this assembly; one functions as part of the host connector.
For that PC Board, you will need the Mill-Max pin&socket connectors.

The way I assembled it was to carefully put the pin connectors on their respective pins, then
position the PC board over top, and solder in place.

Steps to assembly:
1. Trim the leads of the through-hole parts carefully, to minimize the solder "bump" on the underside of the board when mounted.
The through-hole parts to be mounted include the headers (or sockets) for the RP2040 board.
2. Solder carefully, minimizing the amount of "bump" below the board
3. Mount the RP2040 board with the USB connector facing away from the 6 holes which will be used to connect to the host connector board.
4. Connect it to a host computer while holding the "Boot" button down.  This will put it in DFU mode and create a virtual drive
on the host computer.  Drag and drop the *.uf2 firmware file into that folder.  A moment later, the virtual drive should disappear
and after about 1 second, all four LEDs should light up briefly as part of the boot sequence of the memory device.  Disconnect it from USB.
5. This board connects to the first PC Board with a 90-degree male pin header; the short end is
connected to the adapter board, and the long end should be cut flush with the console connector
and soldered in place.  Be careful to connect the 5V pin properly.


## Source Code

### Compilation

This was built using Pico-SDK version 1.3.0, which was just released.  In truth, this had been working with
pico-sdk 1.2.0 and a special version of TinyUSB as of July 4... but I preferred to wait until a standard
development environment was available, as I would prefer not to support the development environment itself.

pico_sdk_import.cmake is from the SDK, but is required by CMake (and thus replicated here).

This is based on the TinyUSB Host HID example, and since this often changes (required by refactoring of
the Host HID code), the initial commit baseline is the source ocde of that example.

To build the source, first ensure that you have the right version of the RaspberryPi/piso-sdk installed.
As this board targets the Adafruit QtPy2040 board, you should run the make_ada.sh script (under UNIX).
Then, "cd build" and "make".

I have also included a release version of the program as a uf2 file in the releases/ folder; just drag and drop it
onto the virtual drive presented when putting the board into BOOTSEL mode (holding the 'boot' button, connect the
board by USB to a host computer, and release the button; a new drive should appear on the computer).

### Theory of Operation

At a high level, this is a multi-processor system, withe the division of work as follows:
- CPU0 : perform USB scanning, accumulate X/Y offsets and button status, and push a word conatining all this to PIO State Machine #1.
At intervals, detect whether the scan interval must be complete (after a certain threshold period).
- CPU1 : watch PIO State Machine #1 for the signal identifying start of scan; set locking to prevent mid-scan updates
- PIO State Machine #1 : Monitor host electrical signals, and send the appropriate bit(s) back to host according to protocol
- PIO State Machine #2 : Watch the trigger line identifying the start of scan, and send the signal back to CPU1

#### PC-FX Mouse/Controller Protocol:

Similar to the NES or SNES, the PC-FX Engine triggers a start-of-read trigger by bringing the LATCH line
low.  The first bit of data is output at this time. After the LATCH signal returns to the high state, each
subsequent bit is sent at the transition of the CLOCK signal to high.

In the below diagram, LATCH is the top trace, CLOCK is the middle trace, and DATA sent back is the bottom trace:
![Top View](../img/FX-mouse.png)


The first bit sent is the least significant, and the most significant bit is sent last.  The last byte in
the sequence is a controller type identifier - 0x2F = Mouse, and 0x0F = Joypad.

The below data diagram shows how the data is internally passed to the state machien for output:


    Structure of the word sent to the FIFO from the ARM:
    |00101111|111111bb|xxxxxxxx|yyyyyyyy| (PCFX mouse)
    |00001111|11111111|1m1mdddd|rsbbbbbb| (PCFX joypad)
    
     Where:
      0 = must be zero
      1 = must be one
     MOUSE:
      b = button values, arranged in right/left sequence for PC-FX use
      x = mouse 'x' movement; right is {1 - 0x7F} ; left is {0xFF - 0x80 } -> but send as 1's complement !
      y = mouse 'y' movement; down  is {1 - 0x7F} ;  up  is {0xFF - 0x80 } -> but send as 1's complement !
    
     NOTES:
      - PC-FX left/right is (-/+) (but sent as complement - i.e. sned 0xFF if value is zero)
      - PC-FX up/down is (-/+) (but sent as complement - i.e. sned 0xFF if value is zero)



## Notes

1. I plan to redesign the boards to support the Seeduino XIAO RP2040, as it is lower-cost than the Adafruit
board.  It is difficult to say at this time, whether either or both of these boards will have sufficient
availability.

2. I am also considering creating a version fo the board using the RP2040 chip directly, and a USB-A connector,
as 99% of mice use the USB-A connector.
