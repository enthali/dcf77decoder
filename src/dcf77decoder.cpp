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
#define COUNT_CLOCK_DEBUG

/* constants */
// bit time of a DCF 0 is 100 ms
#define DCF_ZERO 136
// bit time of a DCF 1 is 200ms
#define DCF_ONE 256
// the "missing" 59th second give a minute pules of > 1800 ms
#define DCF_MIN 1280

// private vairables
unsigned char signal, buffer;                // checking the input signal and keeping a history
unsigned char bit;                           // the current bit in the bitstream
unsigned char dcf77signalPin, dcf77resetPin; // the pins the DCF module is connected to
unsigned long int timeStamp;                 // to calculate the time passed since the last signal change
unsigned int deltaTime;                      // to measure the time between signal changes
// a second timer based on system millis() to advance the seconds and keep the clock running when there's bad signal
unsigned long int secTimer;

// structs to decode the DCF77 bit stream
struct dcfStreamStruct
{
    unsigned long long minStart : 1;       // Minute start bit allways 0
    unsigned long long reserved : 14;      // reserved
    unsigned long long antenna : 1;        // active antenna
    unsigned long long daylightSaving : 1; // announcement daylight saving
    unsigned long long timeZone : 2;       // timezone of the DCF sender
    unsigned long long leapSecond : 1;     // Leap second announcement
    unsigned long long start : 1;          // telegram start bit allways high
    unsigned long long minOnes : 4;        // minutes
    unsigned long long minTens : 3;        // minutes
    unsigned long long parMin : 1;         // parity minutes
    unsigned long long hourOnes : 4;       // hours
    unsigned long long hourTens : 2;       // hours
    unsigned long long parHour : 1;        // parity hours
    unsigned long long dayOnes : 4;        // day
    unsigned long long dayTens : 2;        // day
    unsigned long long weekday : 3;        // day of week
    unsigned long long monthOnes : 4;      // month
    unsigned long long monthTens : 1;      // month
    unsigned long long yearOnes : 4;       // year (5 -> 2005)
    unsigned long long yearTens : 4;       // year
    unsigned long long parDate : 1;        // date parity
};
// buffer to receive the dcf bitstream
unsigned long long dcf77BitStream = 0; // 0xa6a6a6a6a6a6a6a6;
// structure to capture the decoded dcfTime
tinyTime dcfTime;

// private  functions forward declaraions
extern int parityCheck(struct dcfStreamStruct *pDcfMsg); // parity check
extern int decodeTime(struct dcfStreamStruct *pDcfMsg);  // time extraction
extern uint8_t parity(uint8_t value);                    // calcualte the parity of an unsigned 8 bit word
extern void advanceCountClock();
#ifdef DCF_DEBUG_BITSTREAM
extern void printBitstream();
#endif

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
This function is to be called every second, it will simply advance time 
the function must be called more than 1x per second */
void advanceCountClock()
{
    // the function must be called more than 1x per second
    //lets check how many millis have passed since last call
    int delta;
    delta = millis() - secTimer;
    if (delta >= 1000)
    {
        // more than 1000
        // advance the secTimer
        secTimer += delta;

        dcfTime.sec += 1;
        if (dcfTime.sec >= 60)
        {
            dcfTime.sec = 0;
            dcfTime.min += 1;
            // if we got to count minutes, something's wrong with the DCF signal. lets set the time status to internal
            dcfTime.status = STATUS_COUNT;
            if (dcfTime.min >= 60)
            {
                dcfTime.min = 0;
                dcfTime.hour += 1;

                if (dcfTime.hour >= 24)
                {
                    dcfTime.hour = 0;
                    dcfTime.weekDay += 1;
                    if (dcfTime.weekDay >= 8)
                    {
                        dcfTime.weekDay = 0;
                    }
                    // here's where we need to implement day and month advancement ...
                }
            }
        }
#ifdef COUNT_CLOCK_DEBUG
        Serial.print(dcfTime.weekDay);
        Serial.print(" / ");
        Serial.print(dcfTime.hour);
        Serial.print(":");
        Serial.print(dcfTime.min);
        Serial.print(":");
        Serial.print(dcfTime.sec);
        Serial.print("   STATUS: ");
        Serial.print(dcfTime.status);
        Serial.println();
#endif
    }
}

// function to set the current count clock time
void setTime(struct tinyTime tTime)
{
    dcfTime = tTime;
}

// function to get the current clock count time
tinyTime getTime()
{
    return (dcfTime);
}

/* this function hast to be called periodically to sample the input pin 
the shortes signal time the dcf 77 decoder provides is a 100ms signal, to safely detect this signal
the sample function has to be called at least every 50ms */

int dcfCheckSignal()
{
    advanceCountClock(); // lets see if we should advance the time

    int retval = 0;
    signal = digitalRead(dcf77signalPin); // get the current timestamp

    // Check if the signal has changed since the last call.
    if (buffer != signal)
    // Yes, the signal has changed
    {
        // get the current system time, and calculate the time that has elapsed since the last call.
        deltaTime = millis() - timeStamp;

#ifdef DCF_DEBUG_TIMESTAMP
        // Debug output of the signal timestamp
        Serial.print("DCF77 signal : ");
        Serial.print(signal);
        Serial.print(" - delta time : ");
        Serial.print(deltaTime);
        Serial.println(" ");
#endif

#ifdef DCF_DEBUG_RAW_DATA
        // Debug output of the raw DCF77 bit signal
        if (deltaTime < DCF_ZERO)
        {
            Serial.println("DCF Value : 0");
        }
        else if (deltaTime < DCF_ONE)
        {
            Serial.println("DCF Value : 1");
        }
        else if (deltaTime > DCF_MIN)
        {
            Serial.println("DCF Value : Minute Pulse");
        }
#endif
        // the DCF77decode code starts here:
        if (deltaTime < DCF_ZERO)
        {
            // shift the bit stream by 1 to the left but don'T add anything
            dcf77BitStream >>= 1;
        }
        else if (deltaTime < DCF_ONE)
        {
            // shift the bit stream by 1 to the left
            dcf77BitStream >>= 1;
            dcf77BitStream |= 0x8000000000000000;
        }
        else if (deltaTime > DCF_MIN)
        {
            // move the bit stream entirely to the right
            dcf77BitStream >>= 5;
#ifdef DCF_DEBUG_BITSTREAM
            printBitstream();
#endif
            // check if a valid, consistent telegram was received
            if (parityCheck((dcfStreamStruct *)&dcf77BitStream))
            {
                // ok go on
                if (decodeTime((dcfStreamStruct *)&dcf77BitStream))
                {
                    // ok we got a valid signal
                    retval = 1; // everything's fine
                }
                else
                {
                    retval = 0; // the telegram wasn't validated
                }
            }
            else
            {
                // no valid telegram we
            }
        }

        // we are done for this time arround clean up

        // save the signal state for the next call in buffer
        buffer = signal;
        // calculate the current timestamp for the next callF
        timeStamp += deltaTime;
    }
    return (retval);
};

#ifdef DCF_DEBUG_BITSTREAM
void printBitstream()
{
    Serial.println();
    Serial.print("          : ");
    for (int i = 0; i < 8; i++)
    {
        Serial.print("...-...|");
    }
    Serial.println();
    Serial.print("Telegram  : ");
    unsigned long long mask = 0x8000000000000000;
    do
    {
        if (dcf77BitStream & mask)
        {
            Serial.print("1");
        }
        else
        {
            Serial.print("0");
        }
        mask >>= 1;
    } while (mask > 0);
    Serial.println();
}
#endif

/*
* This function checks the parity bits in the DCF77 telegram.
* Return OK if all parity bits are ok
* Return ERR if one parity bit is not ok
*/
int parityCheck(struct dcfStreamStruct *pDcfMsg)
{

#ifdef DCF_DEBUG_PARITY
    Serial.print("Parity Check Minutes : ");
    Serial.println((int)pDcfMsg->parMin);
    Serial.print("Parity Check Hours : ");
    Serial.println((int)pDcfMsg->parHour);
    Serial.print("Parity Check Date : ");
    Serial.println((int)pDcfMsg->parDate);
#endif

    // check the minute parity bit
    if (pDcfMsg->parMin == (parity(pDcfMsg->minOnes) ^ parity(pDcfMsg->minTens)))
    {
#ifdef DCF_DEBUG_PARITY
        Serial.println(" Minute Parity OK ");
#endif
        // check the parity bit of the hours
        if (pDcfMsg->parHour == (parity(pDcfMsg->hourOnes) ^ parity(pDcfMsg->hourTens)))
        {
#ifdef DCF_DEBUG_PARITY
            Serial.println(" Hour Parity OK");
#endif
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
#ifdef DCF_DEBUG_PARITY
                Serial.println(" Date Parity OK");
#endif
                // everything is seems fine, so let's decode the time
                return (1); // everything's fine
            }
            else
            {
#ifdef DCF_DEBUG_PARITY
                Serial.println(" Date Parity ERROR");
#endif
            }
        }
        else
        {
#ifdef DCF_DEBUG_PARITY
            Serial.println(" Hour Parity ERROR");
#endif
        }
    }
    else
    {
#ifdef DCF_DEBUG_PARITY
        Serial.println(" Minute Parity ERROR ");
#endif
    }
    return (0); // we don't have a valid time (yet)
}

// calculate the parity of the lower 4 bit of an unsigned 8 bit word;
uint8_t parity(uint8_t value)
{
#ifdef DCF_DEBUG_PARITY
    Serial.println(value, BIN);
#endif
    value ^= value >> 2; // xor the lower 4 bit result into the lower 2
#ifdef DCF_DEBUG_PARITY
    value &= 3; // to ease debugging kill all above the least significant two bits
    Serial.println(value, BIN);
#endif
    value ^= value >> 1; // yor the lower 2 bits result into lowest bit
    value &= 1;          // parity bit is now the least significant bit, so erase any remaining the rest
#ifdef DCF_DEBUG_PARITY
    Serial.println(value, BIN);
#endif
    return (value);
}

/*
* This function extracts the original time from the DCF77 bit stream and stores it in the clock structure.
* Return OK if all fields could be converted otherwise ERR
*/
int decodeTime(struct dcfStreamStruct *pDcfMsg)
{
#ifdef DCF_DEBUG_DECODE
    Serial.print((int)pDcfMsg->hourTens);
    Serial.print((int)pDcfMsg->hourOnes);
    Serial.print(":");
    Serial.print((int)pDcfMsg->minTens);
    Serial.print((int)pDcfMsg->minOnes);
    Serial.print(" weekday : ");
    Serial.println((int)pDcfMsg->weekday);
#endif
    // so fare so good
    // simple check if the last minute received +1 is the same as this time arround
    if ((pDcfMsg->minOnes + pDcfMsg->minTens * 10) == dcfTime.min + 1)
    {
        // ok looks like we received two telegrams in seqence, therefore set the time status to good
        dcfTime.status = STATUS_DCF;
    }
    else
    {
        // this is apparently the first telegram we received in a sequence
        dcfTime.status = STATUS_FIRST;
    }
    secTimer = millis(); // reset the second counter
    dcfTime.sec = 0;
    dcfTime.min = pDcfMsg->minOnes + pDcfMsg->minTens * 10;
    dcfTime.hour = pDcfMsg->hourOnes + pDcfMsg->hourTens * 10;
    dcfTime.weekDay = pDcfMsg->weekday;
    // set the system time
    if (dcfTime.status == STATUS_DCF)
    {
        setTime(dcfTime);
        return (1); // we have a valid time
    }
    return (0); // no valid time (yet
}