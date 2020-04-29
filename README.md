= DEN4 - MIDI to CV/GATE converter using AD5668 chip and Arduino Nano Every

Simple MIDI to CV/GATE conveter program for Arduino Nano Every and AD5668 octal 16bit D/A converter.

== Curcuit

Outline of the pin connection is as follows.

=== Connection from Arduino to AD5668

    Arduino    <-> AD5668
    D13 (SCK)  <-> 16 (SCLK)
    D10 (SS)   <-> 2  (SYNC)
    D11 (MOSI) <-> 15 (DIN)
    D8         <-> 1  (LDAC)
    D9         <-> 9  (CLR)
    A7         <-> 8  (VREF)

=== Connection from AD5668 to CV output
   
    AD5668      -> CV PORT
    4  (VOUT_A) -> CV2
    13 (VOUT_B) -> CV5
    5  (VOUT_C) -> CV1
    12 (VOUT_D) -> CV6
    6  (VOUT_E) -> CV4
    11 (VOUT_F) -> CV7
    7  (VOUT_G) -> CV3
    10 (VOUT_H) -> CV8
    
=== Connection from Arduino to GATE output

Use negative output in Arduino to use FET (inversion) output buffer.

    Arduino  -> GATE PORT
    A1       -> GATE1
    A0       -> GATE2
    D3       -> GATE3
    D2       -> GATE4
    D6       -> GATE5
    D7       -> GATE6
    D4       -> GATE7
    D5       -> GATE8

=== Connection from photo transister to Ardion and  MIDI connector

    Arduino  <- Photo transister -> MIDI connector / Circuit
    D0_RX    <- Collector
                Emitter          -> GND
                Cathode          -> port 5
                Anode            -> port 4

== MIDI Channels and conversion modes

=== Ch. 1 to 8: CV/GATE mode

This mode is enabled when MIDI message is received on channel 1 to 8. CV 1 to 8 are controlled by Pitch (note number), and GATE 1 to 8 are controlled by NOTE ON/OFF (or velocity 0). Additionally, CVs are controlled by NRPN message (MSB 0x40) too. In this situation, decode 16bit value from NRPN LSB's lower 2bit and DATA MSB (7bit), DATA LSB (7bit), and use it to decide CV voltage. 

 * When receive NOTE ON/OFF for Ch. N, then update CV port N and
   update GATE port N ON/OFF.
 * When receibe NRPN MSB 0x40, then build 16bit data
   by LSB lower 2bit + DATA MSB 7bit + LSB 7bit,
   and use the data to update CV port N.

ex) use Ch. 1 NOTE ON/OFF to control CV1 by pitch, GATE1 by NOTE ON/OFF

=== Ch. 13 to 16: CV/GATE/CV mode

This mode is enabled when MIDI message is received on channel 13 to 16. CV 1 to 4 are controlled by Pitch (note number), and GATE 1 to 4 are controlled by NOTE ON/OFF (or velocity 0), CV 5 to 8 are controlled by Modulation CC.

 * When receive NOTE ON/OFF for Ch. N, then update CV port (N-12)
   and update GATE port (N-12) ON/OFF
 * When receive Modulation CC, then update CV port (N-8)

ex) use Ch. 13 NOTE ON/OFF to control CV1 by pitch, GATE1 by NOTE ON/OFF,
CV5 by modulation wheel

=== Ch. 9: GATE MODE

This mode enabled when MIDI message is received on channel 9. GATE 1 to 8 is controlled by Pitch (note number) and NOTE ON/OFF.

 * When Receive NOTE ON/OFF with note number N, GATE port N+1 ON/OFF.

== USB Serial control

This program also can receive MIDI signals throgh USB serial port (Arduino Nano Every can use USB serial port and dedicated serial port together.)

= Modifications to AD5668 library

This software uses AD5668-Library (https://github.com/bobhart/AD5668-Library), andI added some modifications to work on my hardware well. Diff is as follows.

```Diff
--- ../AD5668-Library/AD5668.cpp	2020-04-29 19:57:57.000000000 +0900
+++ ./AD5668-Library/AD5668.cpp	2020-03-04 00:25:25.000000000 +0900
@@ -28,6 +28,8 @@
 #include "AD5668.h"
 #include <SPI.h>
 
+SPISettings setting(2000000, MSBFIRST, SPI_MODE2);
+
 // Hardware SPI Constructor, must use hardware SPI bus pins
 AD5668::AD5668(uint8_t ssPin, uint8_t clrPin, int8_t ldacPin) {
   _ssPin = ssPin;
@@ -67,7 +69,7 @@ void AD5668::init() {
 	digitalWrite(_mosiPin, LOW);
 	digitalWrite(_sclkPin, LOW);
   }
-  digitalWrite(_ssPin, LOW);
+  digitalWrite(_ssPin, HIGH);
   if (_ldacPin > 0) {
     digitalWrite(_ldacPin, HIGH);
   }
@@ -85,19 +87,25 @@ void AD5668::writeDAC(uint8_t command, u
   digitalWrite(_ssPin, LOW);
   delayMicroseconds(1);
   if (_hwSPI) {
+    SPI.beginTransaction(setting);
+    digitalWrite(_ssPin, LOW);
     SPI.transfer(b1);
     SPI.transfer(b2);
     SPI.transfer(b3);
     SPI.transfer(b4);
+    digitalWrite(_ssPin, HIGH);
+    SPI.endTransaction();
   }
   else {
+    digitalWrite(_ssPin, LOW);
+    delayMicroseconds(1);
 	shiftOut(_mosiPin, _sclkPin, MSBFIRST, b1);
     shiftOut(_mosiPin, _sclkPin, MSBFIRST, b2);
     shiftOut(_mosiPin, _sclkPin, MSBFIRST, b3);
     shiftOut(_mosiPin, _sclkPin, MSBFIRST, b4);
+    delayMicroseconds(1);
+    digitalWrite(_ssPin, HIGH);
   }
-  delayMicroseconds(1);
-  digitalWrite(_ssPin, HIGH);
 }
 
 void AD5668::writeChannel(uint8_t channel, unsigned int value) {
```

= License (GPLv3)

Copyright (c) 2020 mesotokyo <meso@meso.tokyo>

This is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by
the Free Software Foundation,
version 3 of the License, which should be included in this
this distribution. If not, see <http://www.gnu.org/licenses/>.

