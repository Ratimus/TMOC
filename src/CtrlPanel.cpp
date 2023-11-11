
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

HardwareCtrl* pFaders[] =
{
  &Fader1, &Fader2, &Fader3, &Fader4, &Fader5, &Fader6, &Fader7, &Fader8
};


void initADC()
{
  adc0.begin(ADC0_CS); // Chip select pin.
  Serial.printf("adc0 initialized, CS = pin %d\n", ADC0_CS);
}

void initFaders()
{
  Serial.println();
  Serial.println("ADC\tCHAN\tMAXVALUE");
  Serial.print("sliders\t");
  Serial.print(adc0.channels());
  Serial.print("\t");
  Serial.println(adc0.maxValue());
  Serial.println("init faders");
  long t0(micros());
  while (!pFaders[0]->isReady()) { pFaders[0]->service(); }
  long t1(micros());
  Serial.printf("Fader 0 = %d, ready [%d uS]... ", pFaders[0]->read(), t1-t0);
  t1 = micros();
  while (!pFaders[1]->isReady()) { pFaders[1]->service(); }
  long t2(micros());
  Serial.printf("Fader 1 = %d, ready [%d uS]...", pFaders[1]->read(), t2-t1);
  t2 = micros();
  while (!pFaders[2]->isReady()) { pFaders[2]->service(); }
  long t3(micros());
  Serial.printf("Fader 2 = %d, ready [%d uS]...", pFaders[2]->read(), t3-t2);
  t3 = micros();
  while (!pFaders[3]->isReady()) { pFaders[3]->service(); }
  long t4(micros());
  Serial.printf("Fader 3 = %d, ready [%d uS]...\n", pFaders[3]->read(), t4-t3);
  t4 = micros();
  while (!pFaders[4]->isReady()) { pFaders[4]->service(); }
  long t5(micros());
  Serial.printf("Fader 4 = %d, ready [%d uS]...", pFaders[4]->read(), t5-t4);
  t5 = micros();
  while (!pFaders[5]->isReady()) { pFaders[5]->service(); }
  long t6(micros());
  Serial.printf("Fader 5 = %d, ready [%d uS]...", pFaders[5]->read(), t6-t5);
  t6 = micros();
  while (!pFaders[6]->isReady()) { pFaders[6]->service(); }
  long t7(micros());
  Serial.printf("Fader 6 = %d, ready [%d uS]...", pFaders[6]->read(), t7-t6);
  t7 = micros();
  while (!pFaders[7]->isReady()) { pFaders[7]->service(); }
  long t8(micros());
  Serial.printf("Fader 7 = %d, ready [%d uS]...\n", pFaders[7]->read(), t8-t7);
  Serial.printf("ADC 0 ready\n");
}

// int16_t   ZERO(0);
// int16_t * VirtualCtrl::sharedZERO(&ZERO);

// // CLOCK SKEW
// VirtualCtrl cskew_TEST[4] = {
//   VirtualCtrl(pFaders[3], 0, 4, -4),
//   VirtualCtrl(pFaders[3], 0, 4, -4),
//   VirtualCtrl(pFaders[3], 0, 4, -4),
//   VirtualCtrl(pFaders[3], 0, 4, -4)
// };


// VirtualCtrl * STPT_BANK[] = { start_TEST, start_TEST+1, start_TEST+2, start_TEST+3 };
// VirtualCtrl * NDPT_BANK[] = { endpt_TEST, endpt_TEST+1, endpt_TEST+2, endpt_TEST+3 };
// VirtualCtrl * CDIV_BANK[] = { clkdv_TEST, clkdv_TEST+1, clkdv_TEST+2, clkdv_TEST+3 };
// VirtualCtrl * CSKW_BANK[] = { cskew_TEST, cskew_TEST+1, cskew_TEST+2, cskew_TEST+3 };

// ModalCtrl TRACK_PARAM_TEST[4] = {
//   ModalCtrl(4, STPT_BANK),
//   ModalCtrl(4, NDPT_BANK),
//   ModalCtrl(4, CDIV_BANK),
//   ModalCtrl(4, CSKW_BANK)
// };

// ModalCtrl (* pCV)(TRACK_PARAM_TEST);
