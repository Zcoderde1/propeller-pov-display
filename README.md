\# Propeller (POV) LED Display



A spinning display that uses Persistence of Vision to show the message WELCOME followed by a live clock in HH:MM:SS format.



The project is built in two versions of the same design. The Arduino C++ version is easy to build and change. The Embedded C version talks directly to the ATmega328P registers, without the Arduino library.



\## Project folders



\- arduino/pov\_welcome\_clock/pov\_welcome\_clock.ino is the Arduino C++ version.

\- avr\_c/pov\_welcome\_clock\_avr.c is the bare-metal Embedded C version.



\## Parts needed



\- Arduino UNO (ATmega328P), mounted on the rotating arm.

\- A3144 Hall effect sensor, mounted on the rotating arm.

\- A small magnet, fixed on the stationary base.

\- Eight 5 mm LEDs, each with a 330 ohm resistor.

\- A 12 V DC motor, about 2000 rpm, driven through a transistor or MOSFET with a flyback diode.

\- A 12 V DC adapter to power the motor, for a steady speed.

\- A 9 V battery to power the rotating board through the Vin pin.

\- Optional: a DS3231 real-time clock module.



\## Wiring



\- LED bits 0 to 4 connect to pins D3 to D7 (PD3 to PD7).

\- LED bits 5 to 7 connect to pins D8 to D10 (PB0 to PB2).

\- The Hall sensor output connects to pin D2 (INT0). The code turns on the internal pull-up.

\- The optional DS3231 connects to A4 (SDA) and A5 (SCL).



\## How it works



\- The Hall sensor gives one interrupt per revolution. The time between two interrupts is the period of one turn.

\- That period is divided into 120 equal time slices, called columns.

\- In each column the eight LEDs show one vertical slice of a 5 by 7 font.

\- The time comes from a software clock, or from the DS3231 if it is connected. The text is rebuilt once per second.

\- Persistence of vision joins the columns into one readable line: WELCOME and the time.



\## Building the Arduino version



Open pov\_welcome\_clock.ino in the Arduino IDE, select Arduino Uno as the board, and upload. To use the DS3231, change USE\_DS3231 to 1 in the settings at the top of the file.



\## Building the Embedded C version



Go into the avr\_c folder and run make to compile. Then run make flash PORT=COM3 on Windows, or make flash PORT=/dev/ttyACM0 on Linux. This needs avr-gcc, avr-libc and avrdude installed.



\## Changes and modification after test



\- REVERSE\_TEXT makes the text mirror image. Set it to true if the text looks mirrored.

\- FLIP\_ROWS flips the text upside down. Set it to true if the text looks upside down. This option is in the Arduino version only.

\- COLS\_PER\_REV sets how many columns are drawn per turn.

\- TIME\_ADJUST\_S adds seconds to compensate for the delay between compiling and uploading.



\## Notes



\- Without the DS3231, the clock starts from the compile time and restarts when the power is removed.

\- Balance the rotating arm so it does not vibrate at high speed.

\- Keep fingers and loose wires away from the spinning arm.



\## License



MIT

