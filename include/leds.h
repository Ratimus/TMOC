#ifndef LEDS_DOT_H
#define LEDS_DOT_H
#include <Arduino.h>
#include "setup.h"
#include <functional>

// Updates main horizontal LED array to display current pattern (in
// performance mode) or status (in one of the editing modes)
class LedController
{
  void updateFaderLeds();
  void updateMainLeds();

  long long resetBlankTime;
  // Don't light up "locked" faders regardless of bit vals
  uint8_t faderLockStateReg;
  // Hardware interfaces for 74HC595
  OutputRegister<uint16_t>  hw_reg;

public:
  bool enabled;
  LedController();
  void updateAll();
  void blinkOut();

  // TODO...
  void setMain_1(uint8_t bit, bool on = true);
  void setFader_1(uint8_t bit, bool on = true);
  void setMain_all(uint8_t reg);
  void setFader_all(uint8_t reg);
};

#endif