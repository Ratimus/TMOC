#ifndef SET_UP_DOT_H
#define SET_UP_DOT_H
#include <Arduino.h>
#include <Wire.h>
#include "hw_constants.h"
#include <GateIn.h>
#include <RatFuncs.h>
#include "OutputDac.h"
#include <bitHelpers.h>
#include <ClickEncoder.h>
#include "modeCtrl.h"
#include "TuringRegister.h"
#include <OutputRegister.h>
#include "ESP32_New_TimerInterrupt.h"

extern ModeControl mode;

// Sequencer state variables
extern uint8_t OctaveRange;

extern int8_t patternPending;

// Core Shift Register functionality
extern TuringRegister alan;

// Hardware interfaces for 74HC595s
extern OutputRegister<uint16_t>  leds;
extern OutputRegister<uint8_t>   triggers;

void setThingsUp();

#endif