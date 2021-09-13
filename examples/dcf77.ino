#include <dcf77decoder.h>
#include <timeJobs.h>

Job printTimeJob, checkSignalJob;
tinyTime dcfTime;

void setup()
{
    // Module is connected to
    // Pin 2 Signal
    // Pin 3 Power / Reset
    dcfSetup(2, 3);

    Serial.begin(115200);

    checkSignalJob.setInterval(8);
    checkSignalJob.setFunction(&callDcf);

    printTimeJob.setInterval(1000);
    printTimeJob.setFunction(&printTime);
}

void loop()
{
    static long deltaTime;           // timeCheck will return the time passed since last loop. this might be any value from 0 - no time passed to what ever time the last loop took
    deltaTime = timeCheck(millis()); // let's see how much time has past since the last loop

    checkSignalJob.checkTime(deltaTime);
    printTimeJob.checkTime(deltaTime);
}

void callDcf(long time)
{
    dcfCheckSignal();
}

void printTime(long time)
{
    dcfTime = getTime();
    Serial.print("Weekday : ");
    Serial.print(dcfTime.weekDay);
    Serial.print("   Time : ");
    Serial.print(dcfTime.hour);
    Serial.print(":");
    Serial.print(dcfTime.min);
    Serial.print(":");
    Serial.print(dcfTime.sec);
    Serial.print(" - signal status : ");
    Serial.print(dcfTime.status);
    Serial.println();
}