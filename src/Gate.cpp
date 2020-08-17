#include "Gate.hpp"
#include "objects.hpp"
#include "pins.hpp"
#include "switches.hpp"

sGate gate;

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
    ledGreen.shouldBlink = false;
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
    #ifdef LOG_MOVES
    logInfo('O');
    writeLogIntoEeprom();
    #endif
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
    #ifdef LOG_MOVES
    logInfo('C');
    writeLogIntoEeprom();
    #endif
}

#ifdef PRINT
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