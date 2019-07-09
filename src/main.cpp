#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"
#include <BH1750FVI.h>
#include <Servo.h>

//Pins
#define servoPin 52
#define openingEdgePin 2
#define closingEdgePin 3
#define openButtonPin 48
#define closeButtonPin 50

//constants
#define hourClose 20
#define hourOpen 5
#define lightClose 10
#define lightOpen 100
#define closeSpeed 50
#define openSpeed 140
#define checkDelay 1000
#define maxMovingTime 5000

//classes
struct sGate
{
    bool isOpening = false;
    bool isClosing = false;
    bool isOpened = false;
    bool isClosed = false;
    unsigned long startMovingTime;
    void openGate();
    void closeGate();
    bool shouldClose();
    bool shouldOpen();
};

//Objects
BH1750FVI myBH1750(BH1750_DEFAULT_I2CADDR, BH1750_ONE_TIME_HIGH_RES_MODE_2, BH1750_SENSITIVITY_DEFAULT, BH1750_ACCURACY_DEFAULT);
RTC_DS1307 RTC;
Servo servo;
sGate gate;

//Variables
float light;
DateTime date;
unsigned long millisNow;

//Functions declarations
void printDate();
void printLightIntensivity();
void openingEdgeISR();
void closingEdgeISR();

//setup
void setup()
{
    pinMode(openingEdgePin, INPUT_PULLUP);
    pinMode(closingEdgePin, INPUT_PULLUP);
    pinMode(openButtonPin, INPUT_PULLUP);
    pinMode(closeButtonPin, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(openingEdgePin), openingEdgeISR, RISING);
    attachInterrupt(digitalPinToInterrupt(closingEdgePin), closingEdgeISR, RISING);
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
}

//main loop
void loop()
{
    if (millis() - millisNow > checkDelay)
    {
        millisNow = millis();
        printDate();
        printLightIntensivity();
        //if (shouldClose() && (!gate.isClosed))
        if ((light < lightClose) && (!gate.isClosed))
        {
            gate.closeGate();
        }
        //if (shouldOpen() && (!gate.isOpened))
        if ((light > lightOpen) && (!gate.isOpened))
        {
            gate.openGate();
        }
    }
    if ((gate.isOpening || gate.isClosing) && (millis() - gate.startMovingTime > maxMovingTime))
    {
        Serial.println(F("Reached max moving time, safety stop"));
        servo.detach();
        gate.isOpening = false;
        gate.isClosing = false;
    }
    if((digitalRead(openButtonPin) == LOW) && (!gate.isOpened))
        gate.openGate();
    if((digitalRead(closeButtonPin) == LOW) && (!gate.isClosed))
        gate.closeGate();
}

//Functions definitions
void sGate::openGate()
{
    isOpening = true;
    if (!servo.attached())
        servo.attach(servoPin);
    servo.write(openSpeed);
    gate.startMovingTime = millis();
}

void sGate::closeGate()
{
    Serial.println(F("Closing gate"));
    isClosing = true;
    if (!servo.attached())
        servo.attach(servoPin);
    servo.write(closeSpeed);
    gate.startMovingTime = millis();
}

void openingEdgeISR()
{
    if (gate.isOpening)
    {
        Serial.println(F("Opening gate"));
        gate.isOpening = false;
        servo.detach();
        gate.isOpened = true;
        Serial.println(F("opening edge interrupt"));
    }
}
void closingEdgeISR()
{
    if (gate.isClosing)
    {
        gate.isClosing = false;
        servo.detach();
        gate.isClosed = true;
        Serial.println(F("closing edge interrupt"));
    }
}

bool sGate::shouldOpen()
{
    if (date.hour() < hourClose && date.hour() >= hourOpen && light > lightOpen)
        return true;
    else
        return false;
}

bool sGate::shouldClose()
{
    if ((date.hour() >= hourClose || date.hour() < hourOpen) && light < lightClose)
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