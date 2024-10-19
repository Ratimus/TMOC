#ifndef TIMERS_DOT_H
#define TIMERS_DOT_H
#include <Arduino.h>

void setupTimers();
void serviceRunList();

uint8_t getFlashTimer();

std::function<bool()> one_shot(
  long long period,
  std::function<void()> callback = nullptr);

#endif
