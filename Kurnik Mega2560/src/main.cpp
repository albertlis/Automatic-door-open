#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"
#include <BH1750FVI.h>
#include <Servo.h>
#include <stdint.h>
#include <EEPROM.h>

//Pins
const constexpr uint8_t servoPin{A0};
const constexpr uint8_t openingEdgePin{2}, closingEdgePin{3};
const constexpr uint8_t openButtonPin{48}, closeButtonPin{7};

//constants
const constexpr uint16_t checkDelay{1000};
const constexpr uint16_t maxMovingTime{18000};

//classes
struct sGate {
    //variables
    volatile bool isOpening{false};
    volatile bool isClosing{false};
    volatile bool isOpened{false};
    volatile bool isClosed{false};
    volatile bool isSafetyStop{false};
    unsigned long startMovingTime{0};

    //constants
    const static uint8_t hourOpen{4}, hourClose{19};
    const static uint8_t absoluteHourClose{23}, absoluteHourOpen{8};
    const static uint8_t lightClose{10}, lightOpen{100};
    const static uint8_t closeSpeed{120}, openSpeed{60};

    //functions
    void openGate();
    void closeGate();
    bool shouldClose() const;
    bool shouldOpen() const;
    bool shouldAbsoluteClose() const;
    bool shouldAbsoluteOpen() const;
    void safetyStop();
    void printInternalState() const;
};

struct sLog
{
    uint8_t hour{0};
    uint8_t minute{0};
    char move{'\0'}; //O - open C - close
};

//Objects
BH1750FVI myBH1750(BH1750_DEFAULT_I2CADDR, BH1750_ONE_TIME_HIGH_RES_MODE_2, 2.0, BH1750_ACCURACY_DEFAULT);
RTC_DS1307 RTC;
Servo servo;
sGate gate;
sLog logs;

//Variables
float light;
DateTime date;
unsigned long millisNow;
uint16_t eepromAddress{0};
const constexpr uint8_t addressStep{sizeof(sLog)};

//Functions declarations
void printDate();
void printLightIntensivity();
void openingEdgeISR();
void closingEdgeISR();
void logInfo(char move);
void writeLogIntoEeprom();

/**************************************************************
 *                          Setup
 **************************************************************/
void setup() {
    pinMode(openingEdgePin, INPUT_PULLUP);
    pinMode(closingEdgePin, INPUT_PULLUP);
    pinMode(openButtonPin, INPUT_PULLUP);
    pinMode(closeButtonPin, INPUT_PULLUP);
    // pinMode(servoPin, OUTPUT);

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

    if (digitalRead(closingEdgePin) == LOW)
        gate.isClosed = true;
    if (digitalRead(openingEdgePin) == LOW)
        gate.isOpened = true;
}

/***************************************************************
 *                  Loop
 ***************************************************************/
void loop() {
    
    if (millis() - millisNow > checkDelay) {
        millisNow = millis();
        printDate();
        printLightIntensivity();
        gate.printInternalState();

        if (gate.shouldClose() && (!gate.isClosed)) {//if ((light < lightClose) && (!gate.isClosed))
            if ((!(gate.isOpening || gate.isClosing)) && (!gate.isSafetyStop))
                gate.closeGate();
        }
        if (gate.shouldOpen() && (!gate.isOpened)) {//if ((light > lightOpen) && (!gate.isOpened))
            if ((!(gate.isOpening || gate.isClosing)) && (!gate.isSafetyStop))
                gate.openGate();
        }
        /***********************************************************************
         *              Check this code
         ***********************************************************************/
        //Absolute opening or closing if reached time
        //Chceck if should be absolute closed and if is closed
        if (gate.shouldAbsoluteClose() && (!gate.isClosed)) { //if ((light < lightClose) && (!gate.isClosed))
            //chceck if is moving and saftey stop is disabled
            if ((!(gate.isOpening || gate.isClosing)) && (!gate.isSafetyStop))
                gate.closeGate();
        }
        //Chceck if should be absolute opened and is opened
        if (gate.shouldAbsoluteOpen() && (!gate.isOpened)) { //if ((light > lightOpen) && (!gate.isOpened))
            //chceck if is moving and saftey stop is disabled
            if ((!(gate.isOpening || gate.isClosing)) && (!gate.isSafetyStop))
                gate.openGate();
        }
        /***********************************************************************
         *             Finish here
         **********************************************************************/
    }
    if ((gate.isOpening || gate.isClosing) && (millis() - gate.startMovingTime > maxMovingTime))
        gate.safetyStop();

    //Dodać digital read z pozycji bo jeśli jest w krańcowej po resecie to idzie dalej
    if (digitalRead(openButtonPin) == LOW) {
        Serial.println(F("Manual open Pin detected"));
        if ((!gate.isOpening)  && (!gate.isOpened)) {
            gate.openGate();
            gate.isSafetyStop = false;
            Serial.println(F("Manual open"));
        }
    }
    if (digitalRead(closeButtonPin) == LOW) {
        Serial.println(F("Manual close Pin detected"));
        if( (!gate.isClosing) && (!gate.isClosed)) {
            gate.closeGate();
            gate.isSafetyStop = false;
            Serial.println(F("Manual close"));
        }
    }
}

/************************************************************************************
 * 
 *                          Definitions
 * 
 ************************************************************************************/
void sGate::safetyStop() {
    Serial.println(F("Reached max moving time, safety stop"));
    servo.detach();
    isOpening = false;
    isClosing = false;
    isOpened = false;
    isClosed = false;
    isSafetyStop = true;
}

void sGate::openGate() {
    isOpening = true;
    if (!servo.attached())
        servo.attach(servoPin);
    servo.write(openSpeed);
    gate.startMovingTime = millis();

    // logInfo('O');
    // writeLogIntoEeprom();
}

void sGate::closeGate() {
    Serial.println(F("Closing gate"));
    isClosing = true;
    if (!servo.attached())
        servo.attach(servoPin);
    servo.write(closeSpeed);
    gate.startMovingTime = millis();

    // logInfo('C');
    // writeLogIntoEeprom();
}

inline bool sGate::shouldOpen() const {
    //Serial.println(F("Should open function"));
    return (date.hour() < hourClose && date.hour() >= hourOpen && light > lightOpen) ? true : false;
}

inline bool sGate::shouldAbsoluteClose() const {
    //Serial.println(F("Should close function"));
    return ((date.hour() >= absoluteHourClose) || (date.hour() < hourOpen)) ? true : false;
}

inline bool sGate::shouldAbsoluteOpen() const {
    //Serial.println(F("Should open function"));
    return ((date.hour() < hourClose) && (date.hour() >= absoluteHourOpen)) ? true : false;
}

inline bool sGate::shouldClose() const {
    //Serial.println(F("Should close function"));
    return ((date.hour() >= hourClose || date.hour() < hourOpen) && light < lightClose) ? true : false;
}

void sGate::printInternalState() const {
    Serial.print("Is closed: ");
    Serial.print(gate.isClosed);
    Serial.print(" Is opened: ");
    Serial.print(gate.isOpened);
    Serial.print(" Is closing: ");
    Serial.print(gate.isClosing);
    Serial.print(" Is opening: ");
    Serial.print(gate.isOpening);
    Serial.print(" Is safety stop: ");
    Serial.println(gate.isSafetyStop);
}

void openingEdgeISR() {
    // Serial.println(F("Opening ISR"));
    if (gate.isOpening)
    {
        Serial.println(F("Opening gate"));
        gate.isOpening = false;
        servo.detach();
        gate.isOpened = true;
        gate.isClosed = false;
        Serial.println(F("opening edge interrupt"));
    }
}

void closingEdgeISR() {
    // Serial.println(F("Closing ISR"));
    if (gate.isClosing)
    {
        gate.isClosing = false;
        servo.detach();
        gate.isClosed = true;
        gate.isOpened = false;
        Serial.println(F("closing edge interrupt"));
    }
}

void printDate() {
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
    Serial.print('\t');
}

void printLightIntensivity() {
    light = myBH1750.readLightLevel();
    Serial.print(light);
    Serial.println(F(" lx"));
}

void logInfo(char move) {
    date = RTC.now();
    logs.hour = date.hour();
    logs.minute = date.minute();
    logs.move = move;
}

void writeLogIntoEeprom() {
        EEPROM.put(eepromAddress, logs);
    //Unit test
    EEPROM.get(eepromAddress, logs);
    //Serial.println
    eepromAddress += addressStep;
    if(eepromAddress >= ( EEPROM.length() - 1)) 
        eepromAddress = 0;
}