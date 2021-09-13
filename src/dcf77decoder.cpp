/*
dcf77 decoder is a library providing functions to decode the dcf77 signal from an external dcf77 receiver
the decoder features a counting clock that allows to progress the time in case the dcf77 signal cannot be received
*/

#include <Arduino.h>
#include <dcf77decoder.h>

/* Debug switches */
#undef DCF_DEBUG_TIMESTAMP
#undef DCF_DEBUG_RAW_DATA
#undef DCF_DEBUG_BITSTREAM
#undef DCF_DEBUG_PARITY
#undef DCF_DEBUG_DECODE

/* constants */
// bit time of a DCF 0 is 100 ms
#define DCF_ZERO 136
// bit time of a DCF 1 is 200ms
#define DCF_ONE 256
// the "missing" 59th second give a minute pules of > 1800 ms
#define DCF_MIN 1280

// structs to decode the DCF77 bit stream
struct dcfStreamStruct
{
    uint64_t minStart : 1;       // Minute start bit allways 0
    uint64_t reserved : 14;      // reserved
    uint64_t antenna : 1;        // active antenna
    uint64_t daylightSaving : 1; // announcement daylight saving
    uint64_t timeZone : 2;       // timezone of the DCF sender
    uint64_t leapSecond : 1;     // Leap second announcement
    uint64_t start : 1;          // telegram start bit allways high
    uint64_t minOnes : 4;        // minutes
    uint64_t minTens : 3;        // minutes
    uint64_t parMin : 1;         // parity minutes
    uint64_t hourOnes : 4;       // hours
    uint64_t hourTens : 2;       // hours
    uint64_t parHour : 1;        // parity hours
    uint64_t dayOnes : 4;        // day
    uint64_t dayTens : 2;        // day
    uint64_t weekday : 3;        // day of week
    uint64_t monthOnes : 4;      // month
    uint64_t monthTens : 1;      // month
    uint64_t yearOnes : 4;       // year (5 -> 2005)
    uint64_t yearTens : 4;       // year
    uint64_t parDate : 1;        // date parity
};
// buffer to receive the dcf bitstream
uint64_t dcf77BitStream = 0; // 0xa6a6a6a6a6a6a6a6;
// structure to capture the decoded dcfTime
tinyTime dcfInternalTime;

// private global vairables
uint8_t sigBuffer;                           // to store the signal history
unsigned char dcf77signalPin, dcf77resetPin; // the pins the DCF module is connected to
unsigned long int secTimer;                  // a second timer based on system millis() to advance the seconds and keep the clock running when there's bad signal

// temporary "reuseable" variables only used within functions w/o conext outside of any function
unsigned long timeStamp = 0; // to calculate the time passed since the last signal change
int deltaTime = 0;           // to measure the time between signal changes
int retVal = 0;              // generic return valriable

// private  functions forward declaraions
int protoParityCheck(struct dcfStreamStruct *pDcfMsg); // parity check
uint8_t parity(uint8_t value);                         // calcualte the parity of an unsigned 8 bit word
int decodeTime(struct dcfStreamStruct *pDcfMsg);       // time extraction
void advanceCountClock();                              // move the clock ahead
uint8_t signalDecode(unsigned long sysTime);           // decode the raw signal
void buildBitstream(int dcfInfo);                      // assemble the bit stream
int checkBitstream();                                  // validate the bit stream

// dcfSetup requires the signal and the reset pin the dcf module is connected to.
int dcfSetup(uint8_t signalPin, uint8_t resetPin)
{
    dcf77signalPin = signalPin;
    dcf77resetPin = resetPin;
    pinMode(dcf77signalPin, INPUT_PULLUP);
    pinMode(dcf77resetPin, OUTPUT);
    digitalWrite(dcf77resetPin, HIGH);
};

/* advanceClock
This function increments the time by one second
*/
void advanceCountClock()
{
    dcfInternalTime.sec += 1;
    if (dcfInternalTime.sec >= 60)
    {
        dcfInternalTime.sec = 0;
        dcfInternalTime.min += 1;
    }

    if (dcfInternalTime.min >= 60)
    {
        dcfInternalTime.min = 0;
        dcfInternalTime.hour += 1;
    }
    if (dcfInternalTime.hour >= 24)
    {
        dcfInternalTime.hour = 0;
        dcfInternalTime.weekDay += 1;
    }
    if (dcfInternalTime.weekDay >= 8)
    {
        dcfInternalTime.weekDay = 0;
    }
    // here's where we need to implement day and month advancement ...
}

/* 
check how much time passed since the last call. if it's more than a second, go ahead and trigger the second
*/
int checkSecond(unsigned long time)
{
    int secCount = 0;
    // how much time has passed?
    deltaTime = time - secTimer;

    // if  1000 or more msec have passed, let's find out how much time has passed
    for (secCount = 0; deltaTime >= 1000; secCount++)
    {
        deltaTime -= 1000;   // reduce the delta
        secTimer += 1000;    // increase the secTime reference counter
        advanceCountClock(); // count the second
    }
    return (secCount);
}

// function to set the current count clock time
void setTime(struct tinyTime tTime)
{
    dcfInternalTime = tTime;
}

// function to get the current clock count time
tinyTime getTime()
{
    return (dcfInternalTime);
}

/* signalDecode checks if the signal level changed and returns the signal code
0 - a bit value 0 was detected
1 - a bit value 1 was detected
2 - the minute pulse was detected
3 - bit pause detected
4 - no change detected
signalDecode expects the system time in millis and the signal of the input pin
*/
uint8_t signalDecode(unsigned long sysTime, int signal)
{
    // Check if the signal has changed since the last call.
    if (sigBuffer != signal)
    // Yes, the signal has changed
    {
        // save the signal state for the next call in buffer
        sigBuffer = signal;

        // use the current system time, and calculate the time that has elapsed since the last call.
        deltaTime = sysTime - timeStamp;
        // remember this time
        timeStamp = sysTime;

        if (deltaTime < DCF_ZERO)
            return (0);
        else if (deltaTime < DCF_ONE)
            return (1);
        else if (deltaTime > DCF_MIN)
            return (2);
        else
            return (3);
    }
    return (4);
}

/* this function hast to be called periodically to sample the input pin 
the shortes signal time the dcf 77 decoder provides is a 100ms signal, to safely detect this signal
the sample function has to be called at least every 50ms */
int dcfCheckSignal()
{
    // reset the return value
    retVal = 0;
    // get the system time for this calculation
    timeStamp = millis();
    // lets see if we should advance the time
    checkSecond(timeStamp);

    // build the bit stream
    // if we decoded some information the return is either
    // 0 - a 0 bit was detected
    // 1 - a 1 bit was detected
    // 2 - the minute signal was detected
    switch (signalDecode(timeStamp, digitalRead(dcf77signalPin)))
    {
    case 0:
        buildBitstream(0);
        break;
    case 1:
        buildBitstream(1);
        break;
    case 2:

        break;
    default:
        retVal = 0;
        break;
    }
    // calculate the current timestamp for the next callF
    timeStamp += deltaTime;
    return (retVal);
}

/* check if the telegram received is valid */
int checkBitstream()
{
    // reset the return value
    retVal = 0;
    // DCF transmits 59 BITs we use a 64bit storage prep the stream to align with the structure
    dcf77BitStream >>= 5;
    // check if a valid, consistent telegram was received
    if (protoParityCheck((dcfStreamStruct *)&dcf77BitStream))
    {
        // ok go on
        if (decodeTime((dcfStreamStruct *)&dcf77BitStream))
        {
            // ok we got a valid signal
            retVal = 1;
        }
    }
}

/* amend the bitstream with either a 0 or a 1 bit */
void buildBitstream(int dcfInfo)
{
    // move the bit stream one bit to the right
    // inserting a 0 bit on the highest bit
    dcf77BitStream >>= 1;

    if (dcfInfo == 1)
        // set the new bit to 1
        dcf77BitStream |= 0x8000000000000000;
}

/*
* This function checks the parity bits in the DCF77 telegram.
* Return OK if all parity bits are ok
* Return ERR if one parity bit is not ok
*/
int protoParityCheck(struct dcfStreamStruct *pDcfMsg)
{
    // check the minute parity bit
    if (pDcfMsg->parMin == (parity(pDcfMsg->minOnes) ^ parity(pDcfMsg->minTens)))
    {
        // check the parity bit of the hours
        if (pDcfMsg->parHour == (parity(pDcfMsg->hourOnes) ^ parity(pDcfMsg->hourTens)))
        {
            // check the parity bit of the date
            if (pDcfMsg->parDate == ((((((
                                             parity(pDcfMsg->dayOnes) ^
                                             parity(pDcfMsg->dayTens)) ^
                                         parity(pDcfMsg->weekday)) ^
                                        parity(pDcfMsg->monthOnes)) ^
                                       parity(pDcfMsg->monthTens)) ^
                                      parity(pDcfMsg->yearOnes)) ^
                                     parity(pDcfMsg->yearTens)))
            {
                // everything is seems fine, so let's decode the time
                return (1); // everything's fine
            }
        }
    }
    // telegram bad
    dcfInternalTime.status = STATUS_DCF_BAD;
    return (0); // we don't have a valid time
}

// calculate the parity of the lower 4 bit of an unsigned 8 bit word;
uint8_t parity(uint8_t value)
{
    value ^= value >> 2; // xor the lower 4 bit result into the lower 2
    value ^= value >> 1; // yor the lower 2 bits result into lowest bit
    value &= 1;          // parity bit is now the least significant bit, so erase any remaining the rest
    return (value);
}

/*
* This function extracts the original time from the DCF77 bit stream and stores it in the clock structure.
* Return OK if all fields could be converted otherwise ERR
*/
int decodeTime(struct dcfStreamStruct *pDcfMsg)
{
    // simple check. check if the actual time is the same as the DCF signal.
    if ((pDcfMsg->minOnes + pDcfMsg->minTens * 10) == dcfInternalTime.min + 1)
    {
        //telegram plausible, set the time status to good
        dcfInternalTime.status = STATUS_DCF_GOOD;
    }
    else
    {
        // was the last telegram consistent?
        if (dcfInternalTime.status = STATUS_DCF_SINGLE)
        {
            dcfInternalTime.status = STATUS_DCF_GOOD;
        }
        else
        {
            // the telegram is not in sequence, set the status to SINGLE
            dcfInternalTime.status = STATUS_DCF_SINGLE;
        }
    }
    secTimer = millis(); // reset the second counter
    dcfInternalTime.sec = 0;
    dcfInternalTime.min = pDcfMsg->minOnes + pDcfMsg->minTens * 10;
    dcfInternalTime.hour = pDcfMsg->hourOnes + pDcfMsg->hourTens * 10;
    dcfInternalTime.weekDay = pDcfMsg->weekday;

    // set the system time
    if (dcfInternalTime.status == STATUS_DCF_GOOD)
    {
        setTime(dcfInternalTime);
        return (1); // we have a validated time
    }
    return (0); // no valid time
}