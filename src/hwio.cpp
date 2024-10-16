#include "hwio.h"
#include <Arduino.h>
#include <MCP_ADC.h>
#include <RatFuncs.h>
#include <MagicButton.h>
#include "timers.h"


////////////////////////////////////////////////////////////////
//                      BUILT-IN IO
////////////////////////////////////////////////////////////////
// Use the onboard ADCs for external control voltage & the main control
ESP32AnalogRead cvA;        // "CV" input
ESP32AnalogRead cvB;        // "NOISE" input
ESP32AnalogRead turing;     // "LOOP" variable resistor

// Here's the ESP32 DAC output
DacESP32 voltsExp(static_cast<gpio_num_t>(DAC1_CV_OUT));

////////////////////////////////////////////////////////////////
//                      GATE INPUTS
////////////////////////////////////////////////////////////////
// Keep track of Clock and Reset digital inputs
GateInArduino gates(NUM_GATES_IN, GATE_PIN, true);


////////////////////////////////////////////////////////////////
//               HARDWARE SHIFT REGISTER OUTPUTS
////////////////////////////////////////////////////////////////
// Hardware interfaces for 74HC595s
OutputRegister<uint16_t>  leds(SR_CLK, SR_DATA, LED_SR_CS, regMap);
OutputRegister<uint8_t>   triggers(SR_CLK, SR_DATA, TRIG_SR_CS, trgMap);

// Data values for 74HC595s
uint8_t trgRegister(0);     // Gate/Trigger outputs + yellow LEDs

auto triggerTimer(one_shot(0));

// Note: you're still gonna need to clock this before it updates
void setTriggerRegister(uint8_t val)
{
  trgRegister = val;
  triggers.setReg(trgRegister);
}

void handleTriggers()
{
  setTriggerRegister(alan.pulseIt());
  triggerTimer = one_shot(10);
  triggers.clock();
}

void checkTriggersExpired()
{
  if (triggerTimer())
  {
    triggers.allOff();
  }
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
    faderVals[ch] = faderBank[ch]->read();

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
MCP3208 adc0 = MCP3208(SPI_DATA_OUT, SPI_DATA_IN, SPI_CLK);

MultiModeCtrl * faderBank[8];

LedController panelLeds;

void initADC()
{
  adc0.begin(ADC0_CS); // Chip select pin.
  for (auto ch(0); ch < 8; ++ch)
  {
    faderBank[ch] = new MultiModeCtrl(8, &adc0, sliderMap[ch], 12);
  }
  dbprintf("adc0 initialized, CS = pin %d\n", ADC0_CS);
}


// Sets upper and lower bounds for faders based on desired octave range
void setRange(uint8_t octaves)
{
  for (auto fader(0); fader < NUM_FADERS; ++fader)
  {
    faderBank[fader]->setRange(octaves);
  }
  dbprintf("Range set to %u octaves\n", octaves);
}

