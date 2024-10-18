#ifndef LEDS_DOT_H
#define LEDS_DOT_H
#include <Arduino.h>
#include "setup.h"

// Updates main horizontal LED array to display current pattern (in
// performance mode) or status (in one of the editing modes)
class LedController
{
  void updateFaderLeds();
  void updateMainLeds();

  long long resetBlankTime;
  // Don't light up "locked" faders regardless of bit vals
  uint8_t faderLockStateReg;
  std::function<bool()> enableLeds;

public:
  void updateAll();
  LedController();
  void setOneShot();
};

#endif