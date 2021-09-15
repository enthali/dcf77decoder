# dcf77decoder

a dcf 77 signal decoder

This Arduino library supports the decoding of the DCF77 signal.
You need a DCF77 receiver module which is connected to the micro controller (e.g. Arduino) via a signal pin and a reset pin.
The reset pin is used to activate the DCF77 module and can be used by the library to reset the receiver module in case of an error.
The library does not need interrupts or dedicated timers. It only needs the system time in milliseconds from the micocontroller.

Installation
copy the content of this project into your arduino library folder

revision history
v1.0 first public release
    known problems :
        library does not detect if a receiver module not sending any signals any more
