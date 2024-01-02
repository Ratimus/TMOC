#ifndef CTRL_PANEL_H
#define CTRL_PANEL_H

#include <Arduino.h>
#include "SharedCtrl.h"

extern MCP3208 * pADC0;
extern ModalCtrl * pCV;
extern HardwareCtrl* pFaders[NUM_FADERS];

const int8_t HIGH_NOTE(12);
const int8_t LOW_NOTE(0);
constexpr uint8_t NUM_BANKS(8);

void initADC();
void initFaders(bool & running);

#endif