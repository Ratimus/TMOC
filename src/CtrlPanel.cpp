
#include <Arduino.h>
#include "MagicButton.h"
#include "MCP_ADC.h"
#include "TMOC_HW.h"
#include <CtrlPanel.h>

////////////////////////////////////////////////////////////////
//                       FADERS
////////////////////////////////////////////////////////////////
MCP3208 adc0 = MCP3208(SPI_DATA_OUT, SPI_DATA_IN, SPI_CLK);
MCP3208 * pADC0 = &adc0;

HardwareCtrl Fader1 = HardwareCtrl(pADC0, 0, SAMPLE_BUFFER_SIZE);
HardwareCtrl Fader2 = HardwareCtrl(pADC0, 1, SAMPLE_BUFFER_SIZE);
HardwareCtrl Fader3 = HardwareCtrl(pADC0, 2, SAMPLE_BUFFER_SIZE);
HardwareCtrl Fader4 = HardwareCtrl(pADC0, 3, SAMPLE_BUFFER_SIZE);
HardwareCtrl Fader5 = HardwareCtrl(pADC0, 4, SAMPLE_BUFFER_SIZE);
HardwareCtrl Fader6 = HardwareCtrl(pADC0, 5, SAMPLE_BUFFER_SIZE);
HardwareCtrl Fader7 = HardwareCtrl(pADC0, 6, SAMPLE_BUFFER_SIZE);
HardwareCtrl Fader8 = HardwareCtrl(pADC0, 7, SAMPLE_BUFFER_SIZE);

HardwareCtrl* pFaders[NUM_FADERS] =
{
  &Fader1,
  &Fader2,
  &Fader3,
  &Fader4,
  &Fader5,
  &Fader6,
  &Fader7,
  &Fader8
};


void initADC()
{
  adc0.begin(ADC0_CS); // Chip select pin.
}


void initADC_LOUD()
{
  adc0.begin(ADC0_CS); // Chip select pin.
  Serial.printf("adc0 initialized, CS = pin %d\n", ADC0_CS);
}


void initFaders(bool & running)
{
  for (uint8_t ff(0); ff < NUM_FADERS; ++ff)
  {
    while (!pFaders[ff]->isReady())
    {
      pFaders[ff]->service();
    }
  }
  running = true;
}


void initFadersLOUD()
{
  Serial.println();
  Serial.println("ADC\tCHAN\tMAXVALUE");
  Serial.print("sliders\t");
  Serial.print(adc0.channels());
  Serial.print("\t");
  Serial.println(adc0.maxValue());
  Serial.println("init faders");
  long t0(micros());
  while (!pFaders[sliderMap[0]]->isReady()) { pFaders[sliderMap[0]]->service(); }
  long t1(micros());
  Serial.printf("Fader 0 = %d, ready [%d uS]... ", pFaders[sliderMap[0]]->read(), t1-t0);
  t1 = micros();
  while (!pFaders[sliderMap[1]]->isReady()) { pFaders[sliderMap[1]]->service(); }
  long t2(micros());
  Serial.printf("Fader 1 = %d, ready [%d uS]...", pFaders[sliderMap[1]]->read(), t2-t1);
  t2 = micros();
  while (!pFaders[sliderMap[2]]->isReady()) { pFaders[sliderMap[2]]->service(); }
  long t3(micros());
  Serial.printf("Fader 2 = %d, ready [%d uS]...", pFaders[sliderMap[2]]->read(), t3-t2);
  t3 = micros();
  while (!pFaders[sliderMap[3]]->isReady()) { pFaders[sliderMap[3]]->service(); }
  long t4(micros());
  Serial.printf("Fader 3 = %d, ready [%d uS]...\n", pFaders[sliderMap[3]]->read(), t4-t3);
  t4 = micros();
  while (!pFaders[sliderMap[4]]->isReady()) { pFaders[sliderMap[4]]->service(); }
  long t5(micros());
  Serial.printf("Fader 4 = %d, ready [%d uS]...", pFaders[sliderMap[4]]->read(), t5-t4);
  t5 = micros();
  while (!pFaders[sliderMap[5]]->isReady()) { pFaders[sliderMap[5]]->service(); }
  long t6(micros());
  Serial.printf("Fader 5 = %d, ready [%d uS]...", pFaders[sliderMap[5]]->read(), t6-t5);
  t6 = micros();
  while (!pFaders[sliderMap[6]]->isReady()) { pFaders[sliderMap[6]]->service(); }
  long t7(micros());
  Serial.printf("Fader 6 = %d, ready [%d uS]...", pFaders[sliderMap[6]]->read(), t7-t6);
  t7 = micros();
  while (!pFaders[sliderMap[7]]->isReady()) { pFaders[sliderMap[7]]->service(); }
  long t8(micros());
  Serial.printf("Fader 7 = %d, ready [%d uS]...\n", pFaders[sliderMap[7]]->read(), t8-t7);
  Serial.printf("ADC 0 ready\n");
}

VirtualCtrl Fader1_pages[NUM_BANKS] = {
  VirtualCtrl(pFaders[sliderMap[0]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[0]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[0]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[0]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[0]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[0]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[0]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[0]],  6, HIGH_NOTE, LOW_NOTE)
};

VirtualCtrl Fader2_pages[NUM_BANKS] = {
  VirtualCtrl(pFaders[sliderMap[1]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[1]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[1]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[1]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[1]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[1]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[1]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[1]],  6, HIGH_NOTE, LOW_NOTE)
};

VirtualCtrl Fader3_pages[NUM_BANKS] = {
  VirtualCtrl(pFaders[sliderMap[2]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[2]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[2]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[2]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[2]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[2]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[2]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[2]],  6, HIGH_NOTE, LOW_NOTE)
};

VirtualCtrl Fader4_pages[NUM_BANKS] = {
  VirtualCtrl(pFaders[sliderMap[3]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[3]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[3]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[3]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[3]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[3]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[3]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[3]],  6, HIGH_NOTE, LOW_NOTE)
};

VirtualCtrl Fader5_pages[NUM_BANKS] = {
  VirtualCtrl(pFaders[sliderMap[4]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[4]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[4]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[4]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[4]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[4]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[4]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[4]],  6, HIGH_NOTE, LOW_NOTE)
};

VirtualCtrl Fader6_pages[NUM_BANKS] = {
  VirtualCtrl(pFaders[sliderMap[5]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[5]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[5]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[5]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[5]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[5]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[5]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[5]],  6, HIGH_NOTE, LOW_NOTE)
};

VirtualCtrl Fader7_pages[NUM_BANKS] = {
  VirtualCtrl(pFaders[sliderMap[6]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[6]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[6]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[6]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[6]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[6]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[6]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[6]],  6, HIGH_NOTE, LOW_NOTE)
};

VirtualCtrl Fader8_pages[NUM_BANKS] = {
  VirtualCtrl(pFaders[sliderMap[7]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[7]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[7]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[7]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[7]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[7]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[7]],  6, HIGH_NOTE, LOW_NOTE),
  VirtualCtrl(pFaders[sliderMap[7]],  6, HIGH_NOTE, LOW_NOTE)
};

VirtualCtrl * F0_BANK[] = {
  Fader1_pages + 0,
  Fader1_pages + 1,
  Fader1_pages + 2,
  Fader1_pages + 3,
  Fader1_pages + 4,
  Fader1_pages + 5,
  Fader1_pages + 6,
  Fader1_pages + 7
};

VirtualCtrl * F1_BANK[] = {
  Fader2_pages + 0,
  Fader2_pages + 1,
  Fader2_pages + 2,
  Fader2_pages + 3,
  Fader2_pages + 4,
  Fader2_pages + 5,
  Fader2_pages + 6,
  Fader2_pages + 7
};

VirtualCtrl * F2_BANK[] = {
  Fader3_pages + 0,
  Fader3_pages + 1,
  Fader3_pages + 2,
  Fader3_pages + 3,
  Fader3_pages + 4,
  Fader3_pages + 5,
  Fader3_pages + 6,
  Fader3_pages + 7
};

VirtualCtrl * F3_BANK[] = {
  Fader4_pages + 0,
  Fader4_pages + 1,
  Fader4_pages + 2,
  Fader4_pages + 3,
  Fader4_pages + 4,
  Fader4_pages + 5,
  Fader4_pages + 6,
  Fader4_pages + 7
};

VirtualCtrl * F4_BANK[] = {
  Fader5_pages + 0,
  Fader5_pages + 1,
  Fader5_pages + 2,
  Fader5_pages + 3,
  Fader5_pages + 4,
  Fader5_pages + 5,
  Fader5_pages + 6,
  Fader5_pages + 7
};

VirtualCtrl * F5_BANK[] = {
  Fader6_pages + 0,
  Fader6_pages + 1,
  Fader6_pages + 2,
  Fader6_pages + 3,
  Fader6_pages + 4,
  Fader6_pages + 5,
  Fader6_pages + 6,
  Fader6_pages + 7
};

VirtualCtrl * F6_BANK[] = {
  Fader7_pages + 0,
  Fader7_pages + 1,
  Fader7_pages + 2,
  Fader7_pages + 3,
  Fader7_pages + 4,
  Fader7_pages + 5,
  Fader7_pages + 6,
  Fader7_pages + 7
};

VirtualCtrl * F7_BANK[] = {
  Fader8_pages + 0,
  Fader8_pages + 1,
  Fader8_pages + 2,
  Fader8_pages + 3,
  Fader8_pages + 4,
  Fader8_pages + 5,
  Fader8_pages + 6,
  Fader8_pages + 7
};

ModalCtrl FADER_BANK[NUM_FADERS] = {
  ModalCtrl(NUM_BANKS, F0_BANK),
  ModalCtrl(NUM_BANKS, F1_BANK),
  ModalCtrl(NUM_BANKS, F2_BANK),
  ModalCtrl(NUM_BANKS, F3_BANK),
  ModalCtrl(NUM_BANKS, F4_BANK),
  ModalCtrl(NUM_BANKS, F5_BANK),
  ModalCtrl(NUM_BANKS, F6_BANK),
  ModalCtrl(NUM_BANKS, F7_BANK)
};

ModalCtrl (*pCV)(FADER_BANK);

void setRange(uint8_t octaves)
{
  for (auto fader(0); fader < NUM_FADERS; ++fader)
  {
    (pCV + fader)->pACTIVE->updateRange(octaves * 12);
  }
}
