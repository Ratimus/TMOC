#ifndef H_W_I_O_DOT_H
#define H_W_I_O_DOT_H
#include <OutputRegister.h>
#include <ESP32AnalogRead.h>
#include <MagicButton.h>
#include <DacESP32.h>
#include <GateIn.h>
#include <SharedCtrl.h>
#include "leds.h"
#include "hw_constants.h"
#include <memory>
#include "OutputDac.h"


extern ControllerBank faders;

// Abstraction for our 12-bit DAC channels
extern MultiChannelDac output;

// Here's the ESP32 DAC output
extern DacESP32 voltsExp;

// Bit 0 set/clear
extern MagicButton writeHigh;
extern MagicButton writeLow;

// Use the onboard ADCs for external control voltage & the main control
extern ESP32AnalogRead cvA;        // "CV" input
extern ESP32AnalogRead cvB;        // "NOISE" input
extern ESP32AnalogRead turing;     // "LOOP" variable resistor

// Control object for all our leds
extern LedController panelLeds;

class Triggers
{
  uint8_t regVal;    // Gate/Trigger outputs + yellow LEDs
  OutputRegister<uint8_t> hw_reg;

public:
  Triggers();
  // Note: you're still gonna need to clock these before these update
  void setReg(uint8_t val);
  void clock();
  void reset();
};

extern Triggers triggers;
// DAC 0: Faders & register
// DAC 1: Faders & ~register
// DAC 2: abs(DAC 1 - DAC 0)
// DAC 3: DAC 0 if reg & BIT0 else no change from last value
void expandVoltages(uint8_t shiftReg);
bool newReset();
bool newClock();
bool clockDown();
void serviceIO();


#endif
