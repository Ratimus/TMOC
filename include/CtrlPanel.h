// ------------------------------------------------------------------------
// CtrlPanel.h
//
// Oct. 2022
// Ryan "Ratimus" Richardson
// ------------------------------------------------------------------------
#ifndef CTRL_PANEL_H
#define CTRL_PANEL_H

#include <Arduino.h>
#include "SharedCtrl.h"
#include "hw_constants.h"

extern MultiModeCtrl * faderBank[];

const int8_t LOW_NOTE(0);
const int8_t HIGH_NOTE(12);

void initADC();
void setRange(uint8_t octaves);

#endif