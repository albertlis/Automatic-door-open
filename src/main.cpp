#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"
#include <BH1750FVI.h>
#include <Servo.h>
#include <stdint.h>

//Pins
const constexpr uint8_t servoPin{52};
const constexpr uint8_t openingEdgePin{2}, closingEdgePin{3};
const constexpr uint8_t openButtonPin{48}, closeButtonPin{50};

//constants
const constexpr uint8_t hourClose{20}, hourOpen{5};
const constexpr uint8_t lightClose{10}, lightOpen{100};
const constexpr uint8_t closeSpeed{50}, openSpeed{140};
const constexpr uint16_t checkDelay{1000};
const constexpr uint16_t maxMovingTime{5000};
//classes
struct sGate
{
    //variables
    bool isOpening = false;
    bool isClosing = false;
    bool isOpened = false;
    bool isClosed = false;
    bool isSafetyStop = false;
    unsigned long startMovingTime;

    //functions
    void openGate();
    void closeGate();
    bool shouldClose() const;
    bool shouldOpen() const;
    void safetyStop();
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

/**************************************************************
 *                          Setup
 **************************************************************/
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

/***************************************************************
 *                  Loop
 ***************************************************************/
void loop()
{
    if (millis() - millisNow > checkDelay)
    {
        millisNow = millis();
        printDate();
        printLightIntensivity();
        if (gate.shouldClose() && (!gate.isClosed) ) //if ((light < lightClose) && (!gate.isClosed))
        {
            if((!(gate.isOpening || gate.isClosing)) && (!gate.isSafetyStop))
                gate.closeGate();
        }
        if (gate.shouldOpen() && (!gate.isOpened)) //if ((light > lightOpen) && (!gate.isOpened))
        {
            if( (!(gate.isOpening || gate.isClosing)) && (!gate.isSafetyStop))
                gate.openGate();
        }
    }
    if ((gate.isOpening || gate.isClosing) && (millis() - gate.startMovingTime > maxMovingTime))
    {
        gate.safetyStop();
    }
    if((digitalRead(openButtonPin) == LOW) && (!gate.isOpened))
    {
        gate.openGate();
        gate.isSafetyStop = false;
    }
    if((digitalRead(closeButtonPin) == LOW) && (!gate.isClosed))
    {
        gate.closeGate();
        gate.isSafetyStop = false;
    }
}

/************************************************************************************
 * 
 *                          Definitions
 * 
 ************************************************************************************/
void sGate::safetyStop()
{
    Serial.println(F("Reached max moving time, safety stop"));
    servo.detach();
    isOpening = false;
    isClosing = false;
    isSafetyStop = true;
}

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

inline bool sGate::shouldOpen() const
{
    //Serial.println(F("Should open function"));
    return (date.hour() < hourClose && date.hour() >= hourOpen && light > lightOpen) ? true : false;
}

inline bool sGate::shouldClose() const
{
    //Serial.println(F("Should close function"));
    return ((date.hour() >= hourClose || date.hour() < hourOpen) && light < lightClose) ? true : false;
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