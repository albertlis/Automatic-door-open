#ifndef GATE_HPP
#define GATE_HPP
#include <Arduino.h>

struct sGate {
    //variables
    volatile bool isOpening{false};
    volatile bool isClosing{false};
    volatile bool isOpened{false};
    volatile bool isClosed{false};
    volatile bool isSafetyStop{false};
    volatile bool isOpenButtonClicked{false};
    volatile bool isCloseButtonClicked{false};
    unsigned long startMovingTime{0};

    //constants
    const static uint8_t hourOpen{6}, hourClose{18};
    const static uint8_t absoluteHourClose{22}, absoluteHourOpen{8};
    const static uint8_t lightClose{3}, lightOpen{8};
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
#endif