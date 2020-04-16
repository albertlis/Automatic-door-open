#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"
#include <BH1750FVI.h>
#include <Servo.h>
#include <stdint.h>
#include <EEPROM.h>
#include <avr/wdt.h>
#include <Streaming.h>

#define DEBUG
/***********************************************************************
 * 
 *      Przy realizacji wyzerować licznik EEPROMu oraz ustawić datę
 * 
 * *********************************************************************/
//Pins
const constexpr uint8_t servoPin{9};
const constexpr uint8_t openingEdgePin{2}, closingEdgePin{3};
const constexpr uint8_t openButtonPin{7}, closeButtonPin{8};
const constexpr uint8_t batteryVoltagePin{A0};
const constexpr uint8_t ledGreenPin{4}, ledRedPin{12}, ledYellowPin{13};

//constants
const constexpr uint16_t checkDelay{1000};
const constexpr uint16_t maxMovingTime{18000};
const constexpr float batteryDischargeVoltage{2.05};
const constexpr uint8_t eepromCounterAddress{0};

//classes
class cLed {
    private:
        const uint8_t pin;
        uint32_t previousTime;
        bool isTurnedOn;
    public:
        bool shouldBlink;

        cLed(uint8_t p) : pin(p), isTurnedOn(false), shouldBlink(false) {}
        void turnOn();
        void turnOff();
        void blink(uint16_t time);
        void stopBlinking();
};

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
BH1750FVI myBH1750(BH1750_DEFAULT_I2CADDR, BH1750_ONE_TIME_HIGH_RES_MODE_2, 0.5, BH1750_ACCURACY_DEFAULT);
RTC_DS1307 RTC;
Servo servo;
sGate gate;
sLog logs;
cLed ledGreen(ledGreenPin), ledRed(ledRedPin), ledYellow(ledYellowPin);

//Variables
float light;
DateTime date;
unsigned long millisNow;
uint16_t eepromAddress{eepromCounterAddress + sizeof(uint16_t)};
const constexpr uint8_t addressStep{sizeof(sLog)};
uint16_t eepromCounter{0};


//Functions declarations
#ifdef DEBUG
void printDate();
void printLightIntensivity();
void printRtcSqwMode();
#endif
void openingEdgeISR();
void closingEdgeISR();
void logInfo(char move);
void writeLogIntoEeprom();
float checkBatteryVoltage();

/**************************************************************
 *                          Setup
 **************************************************************/
void setup() {
    pinMode(openingEdgePin, INPUT_PULLUP);
    pinMode(closingEdgePin, INPUT_PULLUP);
    pinMode(openButtonPin, INPUT_PULLUP);
    pinMode(closeButtonPin, INPUT_PULLUP);
    pinMode(servoPin, OUTPUT);
    pinMode(ledGreenPin, OUTPUT);
    pinMode(ledRedPin, OUTPUT);
    pinMode(ledYellowPin, OUTPUT);

    attachInterrupt(digitalPinToInterrupt(openingEdgePin), openingEdgeISR, RISING);
    attachInterrupt(digitalPinToInterrupt(closingEdgePin), closingEdgeISR, RISING);

    Serial.begin(115200);
    Wire.begin();

    RTC.begin();
    // RTC.adjust(DateTime(__DATE__, __TIME__));
    RTC.writeSqwPinMode(DS1307_OFF);
    while (myBH1750.begin() != true) {
        #ifdef DEBUG
        Serial.println(F("ROHM BH1750FVI is not present")); //(F()) saves string to flash & keeps dynamic memory free
        #endif
        delay(5000);
    }
    #ifdef DEBUG
    printRtcSqwMode();
    Serial.println(F("Conected with light sensor"));

    Serial.print("EEPROM counter: ");
    eepromCounter = EEPROM.read(eepromCounterAddress);
    Serial.println(eepromCounter + 1);
    for(uint16_t i{0}; i < EEPROM.read(eepromCounterAddress); i += 2) {
        sLog tempLog, tempLog1;
        EEPROM.get(eepromAddress, tempLog);
        eepromAddress += addressStep;
        EEPROM.get(eepromAddress, tempLog1);
        eepromAddress += addressStep;
        Serial << "Godzina: " << tempLog.hour << " Minuta: " << tempLog.minute << " Ruch: " << tempLog.move << '\t'
               << "Godzina: " << tempLog1.hour << " Minuta: " << tempLog1.minute << " Ruch: " << tempLog1.move << endl;
    }
    #endif
    if (digitalRead(closingEdgePin) == LOW)
        gate.isClosed = true;
    if (digitalRead(openingEdgePin) == LOW)
        gate.isOpened = true; 
    wdt_enable(WDTO_1S);
}

/***************************************************************
 *                  Loop
 ***************************************************************/
void loop() {

    if (millis() - millisNow > checkDelay) {
        millisNow = millis();
        #ifdef DEBUG
        printDate();
        printLightIntensivity();
        gate.printInternalState();
        #endif
        if (gate.shouldClose() && (!gate.isClosed)) {
            if ((!(gate.isOpening || gate.isClosing)) && (!gate.isSafetyStop))
                gate.closeGate();
        }
        if (gate.shouldOpen() && (!gate.isOpened)) {
            if ((!(gate.isOpening || gate.isClosing)) && (!gate.isSafetyStop))
                gate.openGate();
        }
       
        if (gate.shouldAbsoluteClose() && (!gate.isClosed)) {
            if ((!(gate.isOpening || gate.isClosing)) && (!gate.isSafetyStop))
                gate.closeGate();
        }
        if (gate.shouldAbsoluteOpen() && (!gate.isOpened)) {
            if ((!(gate.isOpening || gate.isClosing)) && (!gate.isSafetyStop))
                gate.openGate();
        }

        ledYellow.shouldBlink = (checkBatteryVoltage() < batteryDischargeVoltage) ? true : false;
    }
    if ((gate.isOpening || gate.isClosing) && (millis() - gate.startMovingTime > maxMovingTime))
        gate.safetyStop();

    if (digitalRead(openButtonPin) == LOW) {
        #ifdef DEBUG
        Serial.println(F("Manual open Pin detected"));
        #endif
        if ((!gate.isOpening)  && (!gate.isOpened)) {
            gate.openGate();
            gate.isSafetyStop = false;
            #ifdef DEBUG
            Serial.println(F("Manual open"));
            #endif
        }
    }
    if (digitalRead(closeButtonPin) == LOW) {
        #ifdef DEBUG
        Serial.println(F("Manual close Pin detected"));
        #endif
        if( (!gate.isClosing) && (!gate.isClosed)) {
            gate.closeGate();
            gate.isSafetyStop = false;
            #ifdef DEBUG
            Serial.println(F("Manual close"));
            #endif
        }
    }

    if (ledGreen.shouldBlink)
        ledGreen.blink(1000);
    else
        ledGreen.stopBlinking();
    if (ledRed.shouldBlink)
        ledRed.blink(1000);
    else 
        ledRed.stopBlinking();
    if (ledYellow.shouldBlink)
        ledYellow.blink(1000);
    else 
        ledYellow.stopBlinking();

    wdt_reset();
}

/************************************************************************************
 * 
 *                          Definitions
 * 
 ************************************************************************************/
inline void cLed::turnOn() {
    digitalWrite(pin, HIGH);
    isTurnedOn = true;
}
inline void cLed::turnOff() {
    digitalWrite(pin, LOW);
    isTurnedOn = false;
}
void cLed::blink(uint16_t time) {
    if (millis() - previousTime > time) {
        previousTime = millis();
        digitalWrite(pin, !digitalRead(pin));
    }
}

inline void cLed::stopBlinking() {
    if(!isTurnedOn)
        digitalWrite(pin, LOW);
}

void sGate::safetyStop() {
    #ifdef DEBUG
    Serial.println(F("Reached max moving time, safety stop"));
    #endif
    servo.detach();
    isOpening = false;
    isClosing = false;
    isOpened = false;
    isClosed = false;
    isSafetyStop = true;
    ledRed.shouldBlink = true;
    ledGreen.turnOff();
    ledRed.turnOff();
}

void sGate::openGate() {
    if (isClosing) {
        servo.detach();
        isClosing = false;
    }
    isOpening = true;
    if (!servo.attached())
        servo.attach(servoPin);
    servo.write(openSpeed);
    gate.startMovingTime = millis();
    ledGreen.turnOff();
    ledGreen.shouldBlink = true;
    ledRed.shouldBlink = false;
    ledRed.turnOff();

    logInfo('O');
    writeLogIntoEeprom();
}

void sGate::closeGate() {
    #ifdef DEBUG
    Serial.println(F("Closing gate"));
    #endif
    if (isOpening) {
        servo.detach();
        isOpening = false;
    }
    isClosing = true;
    if (!servo.attached())
        servo.attach(servoPin);
    servo.write(closeSpeed);
    gate.startMovingTime = millis();
    ledGreen.turnOff();
    ledGreen.shouldBlink = true;
    ledRed.shouldBlink = false;

    logInfo('C');
    writeLogIntoEeprom();
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
#ifdef DEBUG
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
#endif

void openingEdgeISR() {
    // Serial.println(F("Opening ISR"));
    if (gate.isOpening)
    {
        gate.isOpening = false;
        servo.detach();
        gate.isOpened = true;
        gate.isClosed = false;
        ledGreen.shouldBlink = false;
        ledRed.shouldBlink = false;
        ledGreen.turnOn();
        ledRed.turnOff();
        #ifdef DEBUG
        Serial.println(F("opening edge interrupt"));
        #endif
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
        ledGreen.shouldBlink = false;
        ledRed.shouldBlink = false;
        ledGreen.turnOff();
        ledRed.turnOn();
        #ifdef DEBUG
        Serial.println(F("closing edge interrupt"));
        #endif
    }
}
#ifdef DEBUG
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
#endif
void logInfo(char move) {
    date = RTC.now();
    logs.hour = date.hour();
    logs.minute = date.minute();
    logs.move = move;
}

void writeLogIntoEeprom() {
    EEPROM.update(eepromCounterAddress, eepromCounter++);
    if ( ((eepromCounter * addressStep) + sizeof(uint16_t)) >= EEPROM.length - 1)
    EEPROM.put(eepromAddress, logs);
    eepromAddress += addressStep;
    if(eepromAddress >= ( EEPROM.length() - 1)) 
        eepromAddress = 0;
}

inline float mapfloat(long x, long in_min, long in_max, long out_min, long out_max) {
    return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}

inline float checkBatteryVoltage() {
    #ifdef DEBUG
    Serial.print(F("Battery voltage: "));
    Serial.println(mapfloat(analogRead(batteryVoltagePin), 0, 1023, 0, 5));
    #endif
    return mapfloat(analogRead(batteryVoltagePin), 0, 1023, 0, 5);
}
#ifdef DEBUG
void printRtcSqwMode() {
  Ds1307SqwPinMode mode = RTC.readSqwPinMode();

  Serial.print("Sqw Pin Mode: ");
  switch(mode) {
  case DS1307_OFF:              Serial.println("OFF");       break;
  case DS1307_ON:               Serial.println("ON");        break;
  case DS1307_SquareWave1HZ:    Serial.println("1Hz");       break;
  case DS1307_SquareWave4kHz:   Serial.println("4.096kHz");  break;
  case DS1307_SquareWave8kHz:   Serial.println("8.192kHz");  break;
  case DS1307_SquareWave32kHz:  Serial.println("32.768kHz"); break;
  default:                      Serial.println("UNKNOWN");   break;
  }
}
#endif