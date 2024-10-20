#include "hwio.h"
#include <MCP_ADC.h>
#include <RatFuncs.h>
#include "timers.h"
#include <memory>

////////////////////////////////////////////////////////////////
//                      BUILT-IN IO
////////////////////////////////////////////////////////////////
// Use the onboard ADCs for external control voltage & the main control
ESP32AnalogRead cvA;    // "CV" input
ESP32AnalogRead cvB;    // "NOISE" input
ESP32AnalogRead turing; // "LOOP" variable resistor

// Here's the ESP32 DAC output
DacESP32 voltsExp(static_cast<gpio_num_t>(DAC1_CV_OUT));

////////////////////////////////////////////////////////////////
//                      GATE INPUTS
////////////////////////////////////////////////////////////////
// Keep track of Clock and Reset digital inputs
GateInArduino gates(NUM_GATES_IN, GATE_PIN, true);

#define RESET_FLAG 1
bool newReset()
{
  return gates.readRiseFlag(RESET_FLAG);
}


#define CLOCK_FLAG 0
bool newClock()
{
  return gates.readRiseFlag(CLOCK_FLAG);
}

bool clockDown()
{
  return gates.readFallFlag(CLOCK_FLAG);
}
////////////////////////////////////////////////////////////////
//               HARDWARE SHIFT REGISTER OUTPUTS
////////////////////////////////////////////////////////////////
Triggers triggers;

void Triggers::reset()
{
  hw_reg.reset();
}

Triggers::Triggers() : hw_reg(OutputRegister<uint8_t>(SR_CLK, SR_DATA, TRIG_SR_CS, trgMap))
{
  ;
}

// Note: you're still gonna need to clock this before it updates
void Triggers::setReg(uint8_t val)
{
  regVal = val;
  hw_reg.setReg(regVal);
}

void Triggers::clock()
{
  setReg(alan.pulseIt());
  hw_reg.clock();
  // Turn the triggers off in {TODO: MAAGIC NUMBER} 10 ms
  one_shot(10, [this]()
           { setReg(0); hw_reg.clock(); });
}

////////////////////////////////////////////////////////////////
//                      DAC OUTPUTS
////////////////////////////////////////////////////////////////
MultiChannelDac output(NUM_DAC_CHANNELS);

// DAC 0: Faders & register
// DAC 1: Faders & ~register
// DAC 2: abs(DAC 1 - DAC 0)
// DAC 3: DAC 0 if reg & BIT0 else no change from last value
void expandVoltages(uint8_t shiftReg)
{
  uint16_t faderVals[8];
  uint16_t noteVals[4]{0, 0, 0, 0};

  for (uint8_t ch(0); ch < NUM_FADERS; ++ch)
  {
    faderVals[ch] = faders.read(ch);

    if (bitRead(shiftReg, ch))
    {
      // CV A: Faders & register
      noteVals[0] += faderVals[ch];
    }
    else
    {
      // CV B: Faders & ~register
      noteVals[1] += faderVals[ch];
    }
  }

  // CV C: abs(CV A - CV B)
  if (noteVals[0] > noteVals[1])
  {
    noteVals[2] = noteVals[0] - noteVals[1];
  }
  else
  {
    noteVals[2] = noteVals[1] - noteVals[0];
  }

  // CV D: same as CV A, but only changes when BIT 0 is high
  static uint16_t lastCV_D(noteVals[0]);
  if (bitRead(shiftReg, 0))
  {
    lastCV_D = noteVals[0];
  }
  noteVals[3] = lastCV_D;

  // Write the output values to the external DACs
  for (uint8_t ch(0); ch < 4; ++ch)
  {
    output.setChannelNote(ch, noteVals[ch]);
  }

  // Write the inverse of the pattern register to the internal (8 bit) DAC
  uint8_t writeVal_8(~shiftReg);
  voltsExp.outputVoltage(writeVal_8);
}

////////////////////////////////////////////////////////////////
//                       FADERS
////////////////////////////////////////////////////////////////

LedController panelLeds;

void serviceIO()
{
  mode.service();
  gates.service();
  faders.service();
  writeLow.service();
  writeHigh.service();
}

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