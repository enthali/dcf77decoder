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

#include <Arduino.h>
#include <dcf77decoder.h>

/* constants */

// the signal time may vary by DCF_JITTER
#define DCF_JITTER 32
// bit time of a DCF 0 is 100 ms
#define DCF_ZERO 100
// bit time of a DCF 1 is 200ms
#define DCF_ONE 200

// signal pauses states
#define SIG_NO_SIG 32
#define SIG_ERROR_SPIKE 9
#define SIG_ERROR 8
#define SIG_PAUSE 4
#define SIG_MIN_PULSE 2
#define SIG_BIT_1 1
#define SIG_BIT_0 0

// defines for the signal state machine
// signal state machine BIT state
#define SIG_STATE_BIT 0
// signal state machine PAUSE 0 (900ms) state
#define SIG_STATE_PAUSE_0 1
// signal state machine PAUSE 1 (800ms) state
#define SIG_STATE_PAUSE_1 2

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
uint64_t dcf77BitStream = 0xa061a060a061a060;
// structure to capture the decoded dcfTime
tinyTime dcfInternalTime;

// private module vairables
uint8_t sigBuffer = 0;                 // to store the signal history
uint8_t dcf77signalPin, dcf77resetPin; // the pins the DCF module is connected to
uint32_t secTimer;                     // a second timer based on system millis() to advance the seconds and keep the clock running when there's bad signal
uint32_t sigTimeStamp = 0;             // last signal change system time
uint32_t sysTimeStamp = 0;             // current valid system time
uint8_t sigState = SIG_STATE_BIT;      // signal state macheine state

// temporary "reuseable" variables only used within functions w/o conext outside of any function
int16_t deltaTime = 0; // to measure the time between signal changes
int16_t retVal = 0;    // generic return valriable

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

    Serial.print("Weekday : ");
    Serial.print(dcfInternalTime.weekDay);
    Serial.print("   Time : ");
    Serial.print(dcfInternalTime.hour);
    Serial.print(":");
    Serial.print(dcfInternalTime.min);
    Serial.print(":");
    Serial.print(dcfInternalTime.sec);
    Serial.print(" - time status : ");
    Serial.print(dcfInternalTime.status);
    Serial.print("  Signal State Machine : ");
    Serial.println(sigState);
    Serial.println();

    // here's where day and month advancement should be implemented...
}

/* 
check how much time passed since the last call. if it's more than a second, go ahead and trigger the second
*/
int checkSecond(unsigned long time)
{
    uint8_t secCount = 0;
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
signalDecode expects the system time in millis and the signal of the input pin
*/
uint8_t signalDecode(unsigned long sysTime, int signal)
{
    // reset return value, prep for no signal
    retVal = SIG_NO_SIG;
    // Check if the signal has changed since the last call.

    if (sigBuffer != signal)
    // Yes, the signal has changed
    {
        // signal change detected,
        // save the signal state for the next call in buffer
        sigBuffer = signal;

        // use the current system time, and calculate the time that has elapsed since the last call.
        deltaTime = sysTime - sigTimeStamp;
        sigTimeStamp = sysTime;

        Serial.println(deltaTime);

        switch (sigState)
        {
        case SIG_STATE_BIT:
            // state Bit possible exit events:
            // 100ms -> bit 0 detected -> next state Pause 0
            // 200ms -> bit 1 detected -> next state Pause 1

            // prepare for an error
            retVal = SIG_ERROR;
            sigState = SIG_STATE_BIT;

            // check if delta time is within acceptable margin of DCF_ONE
            if ((deltaTime > (DCF_ZERO - DCF_JITTER)) & (deltaTime < (DCF_ZERO + DCF_JITTER)))
            {
                // Bit 1 detected
                retVal = SIG_BIT_0;
                sigState = SIG_STATE_PAUSE_0;
            }
            if ((deltaTime > (DCF_ONE - DCF_JITTER)) & (deltaTime < (DCF_ONE + DCF_JITTER)))
            {
                // Bit 0 detected
                retVal = SIG_BIT_1;
                sigState = SIG_STATE_PAUSE_1;
            }
            break;

        case SIG_STATE_PAUSE_1:
            // state Pause 1 possible exit events:
            // 800ms -> pause detected -> next state bit
            // 1800ms -> min_sync detected -> next state bit

            // prepare for an error
            retVal = SIG_ERROR;
            sigState = SIG_STATE_BIT;

            // check if delta time is within acceptable margin of a pause after a bit 0 has been received
            if ((deltaTime > (1000 - DCF_ONE - DCF_JITTER)) & (deltaTime < (1000 - DCF_ONE + DCF_JITTER)))
            {
                // Bit 0 detected
                retVal = SIG_PAUSE;
                sigState = SIG_STATE_BIT;
            }

            // check if delta time is within acceptable margin of a pause after a bit 0 and the minute sync has been received
            if ((deltaTime > (2000 - DCF_ONE - DCF_JITTER)) & (deltaTime < (2000 - DCF_ONE + DCF_JITTER)))
            {
                // Bit 0 detected
                retVal = SIG_MIN_PULSE;
                sigState = SIG_STATE_BIT;
            }
            break;

        case SIG_STATE_PAUSE_0:
            // state Pause 0 possible exit events:
            // 900ms -> pause detected -> next state bit
            // 1900ms -> min_sync detected -> next state bit

            // prepare for an error
            retVal = SIG_ERROR;
            sigState = SIG_STATE_BIT;
            // check if delta time is within acceptable margin of a pause after a bit 0 has been received
            if ((deltaTime > (1000 - DCF_ZERO - DCF_JITTER)) & (deltaTime < (1000 - DCF_ZERO + DCF_JITTER)))
            {
                // Bit 0 detected
                retVal = SIG_PAUSE;
                sigState = SIG_STATE_BIT;
            }

            // check if delta time is within acceptable margin of a pause after a bit 0 and the minute sync has been received
            if ((deltaTime > (2000 - DCF_ZERO - DCF_JITTER)) & (deltaTime < (2000 - DCF_ZERO + DCF_JITTER)))
            {
                // Bit 0 detected
                retVal = SIG_MIN_PULSE;
                sigState = SIG_STATE_BIT;
            }
            break;

        default:
            // anything else would be an error
            retVal = SIG_ERROR;
            sigState = SIG_STATE_BIT;
            break;
        }
    }
    return (retVal);
}

/* amend the bitstream with either a 0 or a 1 bit */
void buildBitstream(uint8_t dcfInfo)
{
    // move the bit stream one bit to the right
    // inserting a 0 bit on the highest bit
    dcf77BitStream >>= 1;

    if (dcfInfo == 1)
        // set the new bit to 1
        dcf77BitStream |= 0x8000000000000000;
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
    // telegram error, set status to STATUS_DCF_BAD
    dcfInternalTime.status = STATUS_DCF_BAD;
    return (0); // we don't have a valid time
}

/*
* This function extracts the original time from the DCF77 bit stream and stores it in the clock structure.
* Return OK if all fields could be converted otherwise ERR
*/
int decodeTime(struct dcfStreamStruct *pDcfMsg)
{
    retVal = 0; // reset return value

    // dcf time state machine
    switch (dcfInternalTime.status)
    {
    case STATUS_DCF_BAD:
        // first telegram after a telegram error, set the status to STATUS_DCF_SINGLE
        dcfInternalTime.status = STATUS_DCF_SINGLE;
        break;
    case STATUS_DCF_SINGLE:
        // second telegram after a telegram error, set the status to STATUS_DCF_GOOD
        dcfInternalTime.status = STATUS_DCF_GOOD;
        break;
    case STATUS_DCF_GOOD:
        // Check if current and last telegram are in sequence
        // the internal clock could run slow or fast therefore the internal time could be already the same as the new telegram or one minute older
        deltaTime = pDcfMsg->minOnes + pDcfMsg->minTens * 10;
        if (!((deltaTime == dcfInternalTime.min + 1) || (deltaTime == dcfInternalTime.min)))
        {
            // this telegram was not in sequence, set status back to STATUS_DCF_BAD
            Serial.print("Minute Missmatch : ");
            Serial.print(pDcfMsg->minOnes + pDcfMsg->minTens * 10);
            Serial.print("  :  ");
            Serial.print(dcfInternalTime.min + 1);
            Serial.println();
            dcfInternalTime.status = STATUS_DCF_BAD;
        }
        break;

    default:
        break;
    }

    // set the system time and sync the secTimer
    if (dcfInternalTime.status == STATUS_DCF_GOOD)
    {
        secTimer = sysTimeStamp; // reset the second counter
        dcfInternalTime.sec = 0;
        dcfInternalTime.min = pDcfMsg->minOnes + pDcfMsg->minTens * 10;
        dcfInternalTime.hour = pDcfMsg->hourOnes + pDcfMsg->hourTens * 10;
        dcfInternalTime.weekDay = pDcfMsg->weekday;
        retVal = 1; // we have a validated time
    }
    return (retVal); // no valid time
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
    return (retVal);
}

/* this function hast to be called periodically to sample the input pin 
the shortes signal time the dcf 77 decoder provides is a 100ms signal, to safely detect this signal
the sample function has to be called at least every 50ms */
int dcfCheckSignal()
{
    // reset the return value
    retVal = 0;
    // get the system time for this calculation
    sysTimeStamp = millis();

    // build the bit stream
    // if we decoded some information the return is either
    // 0 - a 0 bit was detected
    // 1 - a 1 bit was detected
    // 2 - the minute signal was detected
    switch (signalDecode(sysTimeStamp, digitalRead(dcf77signalPin)))
    {
    case SIG_BIT_0:
        buildBitstream(0);
        break;

    case SIG_BIT_1:
        buildBitstream(1);
        break;

    case SIG_MIN_PULSE:
        checkBitstream();
        break;

    case SIG_PAUSE:
        break;

    case SIG_ERROR:
        // if there's any bit error set the time status to BAD
        dcfInternalTime.status = STATUS_DCF_BAD;
        break;

    case SIG_ERROR_SPIKE:
        // if there's any bit error set the time status to BAD
        dcfInternalTime.status = STATUS_DCF_BAD;
        break;

    default:
        retVal = 0;
        break;
    }

    // part 2, lets see if we should advance the time
    checkSecond(sysTimeStamp);

    // calculate the current timestamp for the next callF
    sysTimeStamp += deltaTime;
    return (retVal);
}
