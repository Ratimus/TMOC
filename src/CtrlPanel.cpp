
#include <Arduino.h>
#include "MagicButton.h"
#include "MCP_ADC.h"
// #include "TMOC_HW.h"
#include <CtrlPanel.h>

////////////////////////////////////////////////////////////////
//                       FADERS
////////////////////////////////////////////////////////////////
MCP3208 adc0 = MCP3208(SPI_DATA_OUT, SPI_DATA_IN, SPI_CLK);

MultiModeCtrl * faderBank[8];

void initADC()
{
  adc0.begin(ADC0_CS); // Chip select pin.
  for (auto ch(0); ch < 8; ++ch)
  {
    faderBank[ch] = new MultiModeCtrl(8, &adc0, sliderMap[ch], 12);
  }
}


void initADC_LOUD()
{
  adc0.begin(ADC0_CS); // Chip select pin.
  Serial.printf("adc0 initialized, CS = pin %d\n", ADC0_CS);
}


// Sets upper and lower bounds for faders based on desired octave range
void setRange(uint8_t octaves)
{
  for (auto fader(0); fader < NUM_FADERS; ++fader)
  {
    faderBank[fader]->setRange(octaves);
  }
}
