/*
dcf77 decoder is a library providing functions to decode the dcf77 signal from an external dcf77 receiver
the decoder features a counting clock that allows to progress the time in case the dcf77 signal cannot be received

New BSD - License
Copyright 2021 Georg Doll (georg.m.dollqgmail.com)

Redistribution and use in source and binary forms, with or without modification, are permitted 
provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions 
and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions 
and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or 
promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
#ifndef dcf77decoder_h
#define dcf77decoder_h

struct tinyTime
{
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t weekDay; // Monday=0 ... Sunday=7
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t status;
};

// the clock is running on the internal counter
#define STATUS_DCF_BAD 0
// the clock has received only 1 DCF77 signal and has not yet validated the signal
#define STATUS_DCF_SINGLE 1
// the clock is running on the external DCF77 signal
#define STATUS_DCF_GOOD 2

// get the current time structure
extern tinyTime getTime();
// set the timie using the time structure
extern void setTime(struct tinyTime tTime);

// get minute
// set minute
// get hour
// set hour
// .... to be implemented

// dcfSetup requires the signal and the reset pin the dcf module is connected to.
extern int dcfSetup(uint8_t signalPin, uint8_t resetPin);
// the checkSignal() function is to be called regulary, it uses the internal millis system time to measure the delay between signals
extern int dcfCheckSignal();

#endif