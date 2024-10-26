#ifndef SET_UP_DOT_H
#define SET_UP_DOT_H
#include <Arduino.h>
#include "TuringRegister.h"
#include "modeCtrl.h"


extern ModeControl mode;

// Core Shift Register functionality
extern TuringRegister alan;

void setThingsUp();

#endif