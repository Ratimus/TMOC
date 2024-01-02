#pragma once

#include <Arduino.h>
#include "MCP_ADC.h"
#include "SharedCtrl.h"
#include <ClickEncoder.h>
#include <ClickEncoderInterface.h>

// NOTE: GPIO34 doesn't have a pullup; I had to add a HW one
const uint8_t LED_SR_CS         (2);
const uint8_t TRIG_SR_CS        (4);
const uint8_t ENC_A             (5);
const uint8_t SPI_DATA_OUT      (12);
const uint8_t SPI_DATA_IN       (13);
const uint8_t SPI_CLK           (14);
const uint8_t ADC0_CS           (15);
const uint8_t RESET_IN          (16);
const uint8_t CLOCK_IN          (17);
const uint8_t SR_CLK            (18);
const uint8_t PWM_OUT[]         {19, 21, 22, 23};
const uint8_t DAC1_CV_OUT       (25);
const uint8_t TOGGLE_UP         (26);
const uint8_t TOGGLE_DOWN       (27);
const uint8_t ENC_B             (32);
const uint8_t SR_DATA           (33);
const uint8_t ENC_SW            (34);
const uint8_t LOOP_CTRL         (35);
const uint8_t CV_IN_A           (36);
const uint8_t UNUSED_ANALOG     (37);
const uint8_t CV_IN_B           (39);

const uint8_t NUM_CV_INS        (4);
const uint8_t NUM_FADERS        (8);
const uint8_t NUM_CHANNELS      (8);
const uint8_t SAMPLE_BUFFER_SIZE(16);

// const uint8_t bitLeds[]   = {1, 0, 3, 2, 5, 4, 7, 6};
// const uint8_t fdrLeds[]   = {3, 7, 5, 4, 1, 6, 0, 2};

const uint8_t sliderMap[] = {7, 6, 5, 4, 3, 2, 1, 0};
const uint8_t trgMap   [] = {7, 6, 5, 4, 3, 2, 0, 1};
const uint8_t regMap   [] = {3, 7, 5, 4, 1, 6, 0, 2, 9, 8, 11, 10, 13, 12, 15, 14};

const uint16_t DAC_0_CAL[]{0, 393, 802, 1217, 1627, 2041, 2459, 2869, 3279};//409 415 410 414 418 410 410
const uint16_t DAC_1_CAL[]{0, 408, 818, 1230, 1638, 2049, 2470, 2882, 3285};//410 412 408 411 421 412 413
const uint16_t DAC_2_CAL[]{0, 400, 811, 1224, 1631, 2041, 2459, 2872, 3280};//411 413 407 410 418 413 408
const uint16_t DAC_3_CAL[]{0, 394, 804, 1214, 1625, 2036, 2454, 2862, 3270};//410 410 411 411 418 408 408

const uint8_t CAL_TABLE_HIGH_OCTAVE(8);
static const uint16_t *DAC_CAL_TABLES[]{DAC_0_CAL, DAC_1_CAL, DAC_2_CAL, DAC_3_CAL};

const uint8_t ENC_SENSITIVITY       (1);
const bool    ENC_ACTIVE_LOW        (1);
const uint8_t ENC_STEPS_PER_NOTCH   (4);

const uint8_t NUM_GATES_IN          (2);
const uint8_t GATE_PIN[NUM_GATES_IN]{CLOCK_IN, RESET_IN};


const uint16_t uS_TO_mS(1000);
#define ONE_KHZ_MICROS uS_TO_mS // 1000 uS for 1khz timer cycle for encoder

extern uint8_t currentBank;


// uint8_t CHROMATIC[] {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
// uint8_t MAJOR[]     {         0,   2,   2, 1,   2,    2,    2, 1};
// uint8_t PHRYGIAN[]  {                   0, 1,   2,    2,    2, 1,  2,  2};
// uint8_t NAT_MINOR[] {0,    2, 1,   2,   2, 1,   2,    2};
// uint8_t PENT_MINOR[]{0,       3,   2,   2,      3,    2};
// uint8_t HARM_MINOR[]{0,    2, 1,   2,   2, 1,      3, 1};
// uint8_t LOCRIAN[]   {      0, 1,   2,   2, 1,   2,    2,    2};
// uint8_t CHROMATIC[] {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
const uint8_t MAJOR[13]     {0, 0, 2, 2, 4, 5, 6, 7, 7, 9,  9, 11, 12};
const uint8_t PHRYGIAN[13]  {0, 0, 1, 3, 3, 5, 5, 7, 8, 8, 10, 10, 12};
const uint8_t NAT_MINOR[13] {0, 0, 2, 3, 3, 5, 5, 7, 8, 8, 10, 10, 12};
const uint8_t PENT_MINOR[13]{0, 0, 0, 3, 3, 5, 5, 7, 7, 7, 10, 10, 12};
const uint8_t HARM_MINOR[13]{0, 0, 2, 3, 3, 5, 5, 7, 8, 8, 11, 11, 12};
const uint8_t LOCRIAN[13]   {0, 0, 1, 3, 3, 5, 6, 6, 8, 8, 10, 10, 12};

static const uint8_t *SCALES[6]{NAT_MINOR, HARM_MINOR, PHRYGIAN, LOCRIAN, PENT_MINOR, MAJOR};

enum ScaleType
{
  natMinor,
  harmMinor,
  phrygian,
  locrian,
  pentMinor,
  major,
  chromatic,
  wholeTone,
  numScales
};

