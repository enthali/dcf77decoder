# dcf77decoder

This Arduino library supports the decoding of the **DCF77** signal.  
You need  

- a DCF77 receiver module and
- an Arduino micro controller  

connect the  modules reset (or power input) and the signal pin to the microcontroller

The reset pin is used to activate the DCF77 module and can be used by the library to reset the receiver module in case of an error.
The library does not require any interrupts or dedicated timers. It is designed to work in polling mode and only needs the system time in milliseconds from the microcontroller. The library provides a non blocking function that should be called approx. every 10ms to check for signal changes.

Installation
copy the content of this project into your arduino library folder

revision history
v1.1 improved signal detection

    This version includes improved signal detection that enables error detection at the bit level.

    known problems :
        * Library does not detect when a receiver module stops sending signals

v1.0 first public release

    known problems :
        * Library does not detect when a receiver module stops sending signals
        * Error handling only on telegram level
