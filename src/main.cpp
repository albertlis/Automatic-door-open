#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <BH1750FVI.h>
#include <Servo.h>
#include <stdint.h>
#include <EEPROM.h>
// #include <avr/wdt.h>
#include <Streaming.h>
#include <Math.h>
// #include "LowPower.h"
#include "switches.hpp"
#include "objects.hpp"
#include "variables.hpp"
#include "functions.hpp"


/***********************************************************************
 * 
 *      Przy realizacji wyzerować licznik EEPROMu oraz ustawić datę
 *      !!!Aby wgrać program trzeba ręcznie resetować płytkę!!!
 *         Dodać kondensatory na wejścia przycisków bo szum potrafią złapać
 * 
 *      Sygnały:
 *          -zielona ciągły:    poprawnie otwarty
 *          -zielona miga:      w ruchu(obie strony)
 *          -zółta miga:        słaba bateria 
 *          -czerwona ciągły:    poprawnie zamknięty
 *          -czerwona miga:     safetyStop
 * 
 * *********************************************************************/


/**************************************************************
 *                          Setup
 **************************************************************/
void setup() {
    delay(1000);
    pinMode(openingEdgePin, INPUT_PULLUP);
    pinMode(closingEdgePin, INPUT_PULLUP);
    pinMode(openButtonPin, INPUT_PULLUP);
    pinMode(closeButtonPin, INPUT_PULLUP);
    pinMode(servoPin, OUTPUT);
    pinMode(ledGreenPin, OUTPUT);
    pinMode(ledRedPin, OUTPUT);
    pinMode(ledYellowPin, OUTPUT);

    attachInterrupt(digitalPinToInterrupt(openingEdgePin), openingEdgeISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(closingEdgePin), closingEdgeISR, CHANGE);
    #ifdef PRINT
    Serial.begin(115200);
    #endif
    Wire.begin();
    
    while(RTC.begin() != true) {
        delay(5000);
    }
    #ifdef ADJUST_TIME
    RTC.adjust(DateTime(__DATE__, __TIME__));
    #endif
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
    #endif
    if (digitalRead(closingEdgePin) == LOW)
        gate.isClosed = true;
    if (digitalRead(openingEdgePin) == LOW)
        gate.isOpened = true; 
    #ifdef RESET_EEPROM_COUNTER
    EEPROM.update(eepromCounterAddress, 0);
    #endif
    eepromCounter = EEPROM.read(eepromCounterAddress);
    // wdt_enable(WDTO_8S);
    #ifdef PRINT_LOG_LIGHT
    printLightLogFromEeprom();
    #endif
    #ifdef PRINT_LOG_MOVES
    printLogMovesFromEeprom();
    #endif
}

/***************************************************************
 *                  Loop
 ***************************************************************/
void loop() {
    if (millis() - millisNow > checkDelay) {
        millisNow = millis();
        date = RTC.now();
        for (uint8_t i{0}; i < lightTableSize; ++i) {
            lights[i] = myBH1750.readLightLevel();
            delay(300);
        }
        
        #ifdef PRINT
        printDate();
        printLightIntensivity();
        gate.printInternalState();
        #endif
        if (gate.shouldClose() && (!gate.isClosed)) {
            if ((!(gate.isOpening || gate.isClosing)) && (!gate.isSafetyStop))
                if(!gate.isOpenButtonClicked)
                    gate.closeGate();
        }
        if (gate.shouldOpen() && (!gate.isOpened)) {
            if ((!(gate.isOpening || gate.isClosing)) && (!gate.isSafetyStop))
                if(!gate.isCloseButtonClicked)
                    gate.openGate();
        }
       
        if (gate.shouldAbsoluteClose() && (!gate.isClosed)) {
            if ((!(gate.isOpening || gate.isClosing)) && (!gate.isSafetyStop))
                if(!gate.isOpenButtonClicked)
                    gate.closeGate();
                    gate.isCloseButtonClicked = false;
        }
        if (gate.shouldAbsoluteOpen() && (!gate.isOpened)) {
            if ((!(gate.isOpening || gate.isClosing)) && (!gate.isSafetyStop))
                if(!gate.isCloseButtonClicked)
                    gate.openGate();
                    gate.isOpenButtonClicked = false;
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
            gate.isOpenButtonClicked = true;
            gate.isCloseButtonClicked = false;
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
            gate.isCloseButtonClicked = true;
            gate.isOpenButtonClicked = false;
            #ifdef DEBUG
            Serial.println(F("Manual close"));
            #endif
        }
    }

    if (ledGreen.shouldBlink) ledGreen.blink(1000);
    else ledGreen.stopBlinking();
    if (ledRed.shouldBlink) ledRed.blink(1000);
    else ledRed.stopBlinking();
    if (ledYellow.shouldBlink) ledYellow.blink(1000);
    else  ledYellow.stopBlinking();
    // delay(2000);
    // wdt_reset();
    #ifdef LOG_LIGHT
    if (millis() - millisNow > logLightInterval) {
        logLightInfo();
        writeLightLogIntoEeprom();
    }
    #endif
}