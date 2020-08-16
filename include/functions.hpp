#ifndef FUNCTIONS_HPP
#define FUNCTIONS_HPP
#include <Arduino.h>
#include "variables.hpp"
#include "Gate.hpp"
#include "pins.hpp"

#ifdef PRINT
void printDate();
void printLightIntensivity();
#endif
#ifdef DEBUG
void printRtcSqwMode();
#endif
void openingEdgeISR();
void closingEdgeISR();
#ifdef LOG_MOVES
void logInfo(char move);
void writeLogIntoEeprom();
#endif
#ifdef LOG_LIGHT
void logLightInfo();
void writeLightLogIntoEeprom();
#endif
#ifdef PRINT_LOG_LIGHT
void printLightLogFromEeprom();
#endif
#ifdef PRINT_LOG_MOVES
void printLogMovesFromEeprom();
#endif
float checkBatteryVoltage();

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

inline bool sGate::shouldOpen() const {
    //Serial.println(F("Should open function"));
    uint8_t counter{0};
    for(uint8_t i{0}; i < lightTableSize; ++i)
        if (lights[i] > lightClose)
            ++counter;
    return ( (date.hour() < hourClose) && (date.hour() >= hourOpen) && (counter == lightTableSize)) ? true : false;
}

inline bool sGate::shouldClose() const {
    //Serial.println(F("Should close function"));
    uint8_t counter{0};
    for(uint8_t i{0}; i < lightTableSize; ++i)
        if (lights[i] < lightClose)
            ++counter;
    return ((date.hour() >= hourClose || date.hour() < hourOpen) && (counter == lightTableSize)) ? true : false;
}

inline bool sGate::shouldAbsoluteClose() const {
    //Serial.println(F("Should close function"));
    return ((date.hour() >= absoluteHourClose) || (date.hour() < hourOpen)) ? true : false;
}

inline bool sGate::shouldAbsoluteOpen() const {
    //Serial.println(F("Should open function"));
    return ((date.hour() < hourClose) && (date.hour() >= absoluteHourOpen)) ? true : false;
}

#endif