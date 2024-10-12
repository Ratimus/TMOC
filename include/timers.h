#ifndef TIMERS_DOT_H
#define TIMERS_DOT_H
#include <Arduino.h>

// Volatile, 'cause we mess with these in ISRs
extern volatile uint8_t flashTimer;
extern volatile uint16_t millisTimer;

uint8_t getFlashTimer();
void setupTimers();

#endif
