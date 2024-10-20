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

// Keep track of Clock and Reset digital inputs
extern GateInArduino gates;

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

void serviceIO();
// // TODO: decide what controls I want to use to activate this alternate mode
// // Melodic algorithm inspired by Mystic Circuits' "Leaves" sequencer
// void iThinkYouShouldLeaf(uint8_t shiftReg)
// {
//   uint16_t faderVals[8];
//   uint16_t noteVals[4]{0, 0, 0, 0};

//   uint8_t faderMasks[4];
//   faderMasks[0] = shiftReg & 0b00001111;
//   faderMasks[1] = shiftReg & 0b00111100;
//   faderMasks[2] = shiftReg & 0b11110000;
//   faderMasks[3] = shiftReg & 0b11000011;

//   // This is the note we're going to write to the analog scale, invert,
//   // and offset circuit
//   uint16_t internalDacNote(0);

//   // Figure out what we're going to set our 4 external DACs to
//   for (uint8_t ch(0); ch < NUM_FADERS; ++ch)
//   {
//     faderVals[ch] = faderBank[ch]->read();
//     dbprintf("Fader %u: %u\n", ch, faderVals[ch]);
//     uint8_t channelMask(0x01 << ch);
//     for (auto fd(0); fd < 4; ++fd)
//     {
//       if (faderMasks[fd] & channelMask)
//       {
//         noteVals[fd] += faderVals[ch];
//       }
//     }
//     if (shiftReg & channelMask)
//     {
//       internalDacNote += faderVals[ch];
//     }
//   }

//   for (auto ch(0); ch < 4; ++ch)
//   {
//     output.setChannelNote(ch, noteVals[ch]);
//   }

//   // Translate the note to a 12 bit DAC value, then convert that to an 8 bit value
//   // since the internal DAC is only 8 bits
//   // uint16_t writeVal_12(noteToDacVal(0, internalDacNote));
//   // uint8_t writeVal_8(map(writeVal_12, 0, 4096, 0, 256));
//   // voltsExp.outputVoltage(writeVal_8);
//   // dbprintf("writeVal_8 : %u\n", writeVal_8);
// }

#endif
