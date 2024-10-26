// ------------------------------------------------------------------------
//                       Turing Machine On Crack
//
//   A software re-imagining of Tom Whitwell / Music Thing Modular's Turing
//   Machine random looping sequencer, plus Voltages and Pulses expanders.
//
//   Based on an ESP32 microcontroller and featuring four 12-bit DACs, one
//   8-bit DAC with analog scale and offset and an inverting output, and
//   eight gate/trigger outputs
//
// main.cpp
//
// Nov. 2022
// Ryan "Ratimus" Richardson
// ------------------------------------------------------------------------
#include <Arduino.h>
#include "setup.h"
#include "hwio.h"


void setup()
{
  setThingsUp();
}


void loop()
{
  handleMode();
  handleToggle();
  handleReset();
  handleClock();
  panelLeds.updateAll();
}
