#ifndef LEDS_DOT_H
#define LEDS_DOT_H
#include <Arduino.h>
#include "setup.h"
#include "hwio.h"

// Updates main horizontal LED array to display current pattern (in
// performance mode) or status (in one of the editing modes)
void updateRegLeds(bool en = true);

#endif