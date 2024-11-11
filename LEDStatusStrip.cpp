﻿#include <LEDStatusStrip.h>
#include <arduino.h>
#define SHORT_BLINK 100
#define LONG_BLINK 500

LEDStatusStrip theLEDStatusStrip;

void LEDStatusStrip::SetPins(int firstpin, int pinCount)
{
	LEDCount = pinCount;
    BasePin = firstpin; 
    int x;
    for (x = 0; x < LEDCount; x++)
    {
        pinMode(BasePin + x, OUTPUT);
    }

	
}
void LEDStatusStrip::SetStatus(int newStatus)
{
    if (LEDCount == 0)
    {
        return;
    }
    int x;

    for (x = 0; x < LEDCount; x++)
    {
        if (newStatus > x)
        {
            digitalWrite(BasePin + x, HIGH);
        }
        else
        {
            digitalWrite(BasePin + x, LOW);
        }
    }
    oldStatus = newStatus;
};
void LEDStatusStrip::Warning(int WarningLevel)
{
    BlinkEm(WarningLevel, LONG_BLINK);
    delay(1000);
    SetStatus(oldStatus);
};
void LEDStatusStrip::FatalError(int ErrorLevel)
{
    while (1)
    {
        BlinkEm(ErrorLevel, SHORT_BLINK);
        delay(1000);
    }

};
void LEDStatusStrip::BlinkEm(int number, unsigned long duration)
{
    int x;
    int y;
    if (LEDCount == 0)
    {
        return;
    }
    for (y = 0; y < number; y++)
    {
        for (x = 0; x < LEDCount; x++)
        {
            digitalWrite(BasePin + x, HIGH);
        }
        delay(duration/2);
        for (x = 0; x < LEDCount; x++)
        {
            digitalWrite(BasePin + x, LOW);
        }
        delay(duration/2);
    }

}