/* dcf77 unit tests */

#include <dcf77decoder.h>
#include <AUnit.h>

using namespace aunit;

// variables to test the time functions
tinyTime testTime1, testTime2;

void setup()
{
    // Module is connected to
    // Pin 2 Signal
    // Pin 3 Power / Reset
    dcfSetup(2, 3);

    Serial.begin(115200);
}

// Enable access to internal module functions
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

extern void advanceCountClock();
extern int checkSecond(unsigned long time);
extern unsigned long long dcf77BitStream;

void loop()
{
    TestRunner::run();
}

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

test(advanceCountClock)
{
    //set test time1
    testTime1.sec = 58;
    testTime1.min = 59;
    testTime1.hour = 23;
    testTime1.weekDay = 2;
    testTime1.status = 2; // pretend we have a valid dcf signal
    setTime(testTime1);

    // one tick
    advanceCountClock();
    testTime2 = getTime();
    assertEqual(testTime2.sec, 59);
    assertEqual(testTime2.status, 2);

    // one more tick we should jump the minute and the hour
    advanceCountClock();
    testTime2 = getTime();
    // expect time to be 3 0:0:0
    assertEqual(testTime2.sec, 0);
    assertEqual(testTime2.min, 0);
    assertEqual(testTime2.hour, 0);
    assertEqual(testTime2.weekDay, 3);
    assertEqual(testTime2.status, 2);
}

test(setGetTime)
{
    // set a random time
    testTime1.sec = 4;
    testTime1.min = 33;
    testTime1.hour = 21;
    // clear time2
    testTime2.sec = 0;
    testTime2.min = 0;
    testTime2.hour = 0;
    // set time1
    setTime(testTime1);
    // get result in time 2
    testTime2 = getTime();
    // check sec,min,hour
    assertEqual(testTime1.sec, testTime2.sec);
    assertEqual(testTime1.min, testTime2.min);
    assertEqual(testTime1.hour, testTime2.hour);
}

extern unsigned long secTimer;

test(checkSecond)
{
    assertEqual(checkSecond(0), 0);          // Set internal time to 0, whatever the return is can be ignored at this time
    assertEqual(checkSecond(500), 0);        // should still be 0
    assertEqual(checkSecond(1000), 1);       // internal time should now be 1001 and we should see 1 sec as result
    assertEqual(checkSecond(3001), 2);       // expect 2 sec to pass
    assertEqual(checkSecond(4000), 1);       // expect another second
    assertEqual(checkSecond(7500), 3);       // should be 3 sec
    assertEqual(checkSecond(30000), 23);     // this should overrunn delta time
    secTimer = 0xfffff000;                   // provoke a timer overflow
    assertEqual(checkSecond(0xffffff00), 3); // very close to the overflow
    assertEqual(checkSecond(1100), 2);       // c2 sec over
}