#ifndef CTRL_PANEL_H
#define CTRL_PANEL_H

#include <Arduino.h>
#include "SharedCtrl.h"

extern MCP3208 * pADC0;
extern ModalCtrl * pCV;
extern HardwareCtrl* pFaders[NUM_FADERS];

void initADC();
void initFaders();

#endif