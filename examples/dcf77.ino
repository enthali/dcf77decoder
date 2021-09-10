#include <dcf77decoder.h>

void setup()
{
    // Module is connected to
    // Pin 2 Signal
    // Pin 3 Power / Reset
    dcfSetup(2, 3);

    Serial.begin(115200);
}

void loop()
{
    dcfCheckSignal();
    delay(8);
}