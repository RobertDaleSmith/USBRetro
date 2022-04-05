# PC_Engine_RP2040_Projects - Projects designed for PC Engine, using RPi Pico or other RP2040 hardware

## [Memory Base 128 reimplementation](Membase/Readme.md)

This is the third implementation of the Memory Base 128 I have written for modern hardware, and the easiest code to read.
The PIOs areused for edge-sensing of the data input, and the ARM core is used for processing.

The data is loaded into SRAM at startup, and saved into Flash after transactions take place.  This may not be ideal, as the
device is not capable of processing additional commands while the Flash flush is in progress (it takes place 0.75 seconds
after the last read/write of a group of read or write transactions).
<img src="https://github.com/dshadoff/PC_Engine_RP2040_Projects/blob/main/img/mini128.jpg" width="355" height="331">

## [PC Engine USB Mouse adapter](PCEMouse/Readme.md)

This allows a modern USB mouse to be used by a PC Engine machine.  The PC Engine did have a mouse later in the console's
lifetime; it was a mouse with a rotating ball and a cord (anybody familiar with this type of mouse would be familiar
with the desire to use a modern cordless optical mouse).

The RP2040 board is used as the USB Host for the mouse, and PIOs are used for signalling to the PC Engine console.
<img src="https://github.com/dshadoff/PC_Engine_RP2040_Projects/blob/main/img/pcemouse.jpg" width="378" height="504">

## [PC-FX USB Mouse adapter](PCFXMouse/Readme.md)

This allows a modern USB mouse to be used by a PC-FX machine.  The PC-FX did have a mouse which could be used on various
games, or even on the startup screen.  It was a mouse with a rotating ball and a cord (anybody familiar with this type of
mouse would be familiar with the desire to use a modern cordless optical mouse).

The RP2040 board is used as the USB Host for the mouse, and PIOs are used for signalling to the PC-FX console.
<img src="https://github.com/dshadoff/PC_Engine_RP2040_Projects/blob/main/img/fxmouse.jpg" width="378" height="504">

## Others

Other Projects are coming... the PIOs on the RPi Pico are very interesting and flexible, so will be well-suited to
retrocomputing/retrogaming projects.

## Notes
I have tried to use the Adafruit QtPy RP2040 as much as possible, as it is a compact form factor which is easy to design around.

Unfortunately, the Adafruit site tries to direct all users toward CircuitPython rather than C/C++ (the Pi SDK), and
as a result, Pinout pages of the "Pinout" page for their RP2040 devices don't include references to the GPIO numbers.

Therefore, I am including a graphic here:

![Adafruit QtPy RP2040 GPIO pinout](img/qtpy_rp2040_GPIO.png)

