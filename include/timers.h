#ifndef TIMERS_DOT_H
#define TIMERS_DOT_H
#include <Arduino.h>

uint8_t getFlashTimer();
void setupTimers();
std::function<bool()> one_shot(long long period, std::function<void()> callback = nullptr);
void serviceRunList();

#endif
