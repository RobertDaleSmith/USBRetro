# PC_Engine_RP2040_Projects - PC Engine Mouse converter

## Overview

This project is intended to allow use of a standard USB mouse on a PC Engine.
While the code and board are in working condition, there is likely still room for improvement,
as the TinyUSB library and pico-sdk are still evolving, and the board could be improved if rebuilt from
basic parts, and using a USB-A connector.


## PC Board and Assembly

I designed all boards using the free version of EAGLE (2-layer, less than 100mm on both X- and Y- axes).
The gerbers are included in this repository, in case you want to get your own set made.

I have included the gerbers and relevant bom.csv and assembly.csv files to get these boards
assembled by JLCPCB, but you will still need the following parts:
- (1) Adafruit QtPy RP2040 microcontroller board
- (2) 7-pin headers.  You may also want to use female pin header sockets in case you need to remove the microcontroller board.
I recommend using short/low-profile header sockets in this case.

Steps to assembly:
1. Trim the leads of the through-hole parts carefully, to minimize the solder "bump" on the underside of the board when mounted.
The through-hole parts to be mounted include the 8-pin mini-DIN socket, and the headers (or sockets) for the RP2040 board.
2. Solder carefully, minimizing the amount of "bump" below the board
3. Mount the RP2040 board with the USB connector facing away from the 8-pin DIN connector.
4. Connect it to a host computer while holding the "Boot" button down.  This will put it in DFU mode and create a virtual drive
on the host computer.  Drag and drop the *.uf2 firmware file into that folder.  A moment later, the virtual drive should disappear
and after about 1 second, all four LEDs should light up briefly as part of the boot sequence of the memory device.  Disconnect it from USB.


## Source Code

### Compilation

This was updated to use Pico-SDK version 1.5.0 .

pico_sdk_import.cmake is from the SDK, but is required by CMake (and thus replicated here)

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
- CPU1 : watch PIO State Machine #1 for the signal identifying start of scan, set locking to prevent mid-scan updates, and to advance the state machine to transmit the 'next nybble' in the sequence.
- PIO State Machine #1 : Monitor host electrical signals, and send the appropriate bit(s) back to host according to protocol
- PIO State Machine #2 : Watch the trigger line identifying the start of scan, and send the signal back to CPU1

#### PCE Mouse Protocol:

The PC Engine triggers a start-of-read trigger by setting the CLR line high (usually with
the SEL line also high).  This resets the multitap, but can also have special meaning on other
peripherals.  On the Mouse, it initiates a scan of the mouse values.

During scan, the mouse will always return the value of the buttons when the SEL line is low.
However, when SEL is high, there is a mini-state machine returning different values:
- The high nybble (4 bits) of the X value of the mouse (since last scan) is sent back after the first high-CLR
- The low nybble of the X value of the mouse is sent back after the second high-CLR
- The 3rd iteration of high-CLR will cause the high nybble of the Y value to be returned
- Finally, the 4th high-CLR will return the low nybble of Y

This state machine is emulated here by sending an accumulated transaction in the format below, and
implementing the state machine (to decide which nybble to return) in the PIO state machine.

     Structure of the word sent to the FIFO from the ARM:
     |00000000|00ssbbbb|xxxxxxxx|yyyyyyyy

     Where:
      0 = must be zero
      s = state (which nybble to output, 3/2/1/0)
      b = button values, arranged in Run/Sel/II/I sequence for PC Engine use
      x = mouse 'x' movement; left is {1 - 0x7F} ; right is {0xFF - 0x80 }
      y = mouse 'y' movement;  up  is {1 - 0x7F} ; down  is {0xFF - 0x80 }
 


## Notes

1. If you would like to enable the functionality to swap mouse buttons when the middle button is pushed (Lemmings'
buttons are the reverse of what you would expect), uncomment the line in hid_app.c which defines "MID_BUTTON_SWAPPABLE"

2. I updated the project some time ago to take advantage of the Adafruit KB2040 board, which breaks out the USB D- and D+
lines, to allow alternate USB connectors (USB-A are the most common connectors for mice).

3. I am also considering creating a version fo the board using the RP2040 chip directly.
