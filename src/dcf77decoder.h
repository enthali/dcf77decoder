/*
dcf77 decoder is a library providing functions to decode the dcf77 signal from an external dcf77 receiver
the decoder features a counting clock that allows to progress the time in case the dcf77 signal cannot be received
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
#define STATUS_COUNT 0
// the clock is running on the external DCF77 signal
#define STATUS_DCF 1
// the clock has received only 1 DCF77 signal and has not yet validated the signal
#define STATUS_FIRST 2

// advance the count clock
extern void secTick(long timeDelta);

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