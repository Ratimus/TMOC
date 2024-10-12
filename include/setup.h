#ifndef SET_UP_DOT_H
#define SET_UP_DOT_H
#include <Arduino.h>
#include <Wire.h>
#include "hw_constants.h"
#include <GateIn.h>
#include <RatFuncs.h>
#include "OutputDac.h"
#include <CtrlPanel.h>
#include <bitHelpers.h>
#include <ClickEncoder.h>
#include "modeCtrl.h"
#include "TuringRegister.h"
#include <OutputRegister.h>
#include "ESP32_New_TimerInterrupt.h"

extern float ONE_OVER_ADC_MAX;

extern ModeControl mode;

// Sequencer state variables
extern uint8_t OctaveRange;
extern uint8_t currentBank;

extern int8_t patternPending;

extern bool set_bit_0;
extern bool clear_bit_0;

// Core Shift Register functionality
extern TuringRegister alan;

// Hardware interfaces for 74HC595s
extern OutputRegister<uint16_t>  leds;
extern OutputRegister<uint8_t>   triggers;


// Don't light up "locked" faders regardless of bit vals
extern uint8_t faderLockStateReg;
extern bool    faderLocksChanged;

void setThingsUp();

#endif