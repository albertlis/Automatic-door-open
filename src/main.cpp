#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"
#include <BH1750FVI.h>
#include <Servo.h>

#define servoPin 52
#define hourClose 20
#define hourOpen 5
#define lightClose 10
#define lightOpen 50

BH1750FVI myBH1750(BH1750_DEFAULT_I2CADDR, BH1750_ONE_TIME_HIGH_RES_MODE_2, BH1750_SENSITIVITY_DEFAULT, BH1750_ACCURACY_DEFAULT);
RTC_DS1307 RTC;
Servo servo;

float light;
DateTime date;

void printDate();
void printLightIntensivity();
bool shouldClose();
bool shouldOpen();

void setup() {
    Serial.begin(115200);
    Wire.begin();
    
    RTC.begin();
    RTC.adjust(DateTime(__DATE__, __TIME__));
    
    while (myBH1750.begin() != true)
    {
        Serial.println(F("ROHM BH1750FVI is not present")); //(F()) saves string to flash & keeps dynamic memory free
        delay(5000);
    }
    Serial.println(F("Conected with light sensor"));

    servo.attach(servoPin);
}

void loop() {
    printDate();
    printLightIntensivity();
    if(shouldClose())
        Serial.println(F("Closing gate"));
    if(shouldOpen())
        Serial.println(F("Opening gate"));


    delay(1000);
}


bool shouldOpen()
{
    if(date.hour() < hourClose && date.hour() >= hourOpen && light > lightOpen)
        return true;
    else
        return false;   
}

bool shouldClose()
{
    if(( date.hour() >= hourClose || date.hour() < hourOpen ) && light < lightClose)
        return true;
    else
        return false;  
}

void printDate()
{
    date = RTC.now();
    Serial.print(date.month(), DEC);
    Serial.print('/');
    Serial.print(date.day(), DEC);
    Serial.print('/');
    Serial.print(date.year(), DEC);
    Serial.print(' ');
    Serial.print(date.hour(), DEC);
    Serial.print(':');
    Serial.print(date.minute(), DEC);
    Serial.print(':');
    Serial.print(date.second(), DEC);
    Serial.println();
}

void printLightIntensivity()
{
    light = myBH1750.readLightLevel();
    Serial.print(light);
    Serial.println(" lx");
}