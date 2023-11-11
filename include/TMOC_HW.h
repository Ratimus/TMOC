#pragma once

#include <Arduino.h>
#include "MCP_ADC.h"
#include "SharedCtrl.h"
#include <ClickEncoder.h>
#include <ClickEncoderInterface.h>

// NOTE: GPIO34 doesn't have a pullup; I had to add a HW one
const uint8_t LED_SR_CS     (2);
const uint8_t TRIG_SR_CS    (4);
const uint8_t ENC_A         (5);
const uint8_t SPI_DATA_OUT  (12);
const uint8_t SPI_DATA_IN   (13);
const uint8_t SPI_CLK       (14);
const uint8_t ADC0_CS       (15);
const uint8_t CLOCK_IN      (16);
const uint8_t RESET_IN      (17);
const uint8_t SR_CLK        (18);
const uint8_t PWM_OUT[] =   {19, 21, 22, 23};
const uint8_t DAC1_CV_OUT   (25);
const uint8_t TOGGLE_UP     (26);
const uint8_t TOGGLE_DOWN   (27);
const uint8_t ENC_B         (32);
const uint8_t SR_DATA       (33);
const uint8_t ENC_SW        (34);
const uint8_t LOOP_CTRL     (35);
const uint8_t CV_IN_A       (36);
const uint8_t UNUSED_ANALOG (37);
const uint8_t CV_IN_B       (39);

const uint8_t NUM_CV_INS        (4);
const uint8_t NUM_FADERS        (8);
const uint8_t SAMPLE_BUFFER_SIZE(16);

const uint8_t chSliders[] = {7, 6, 5, 4, 3, 2, 1, 0};
// const uint8_t bitLeds[]   = {1, 0, 3, 2, 5, 4, 7, 6};
// const uint8_t fdrLeds[]   = {3, 7, 5, 4, 1, 6, 0, 2};

const uint8_t trgMap[] = {7, 6, 5, 4, 3, 2, 0, 1};
const uint8_t regMap[] = {3, 7, 5, 4, 1, 6, 0, 2, 9, 8, 11, 10, 13, 12, 15, 14};


const uint8_t ENC_SENSITIVITY       (1);
const bool    ENC_ACTIVE_LOW        (1);
const uint8_t ENC_STEPS_PER_NOTCH   (4);

const uint8_t NUM_GATES_IN          (2);
const uint8_t GATE_PIN[NUM_GATES_IN]{CLOCK_IN, RESET_IN};


const uint16_t uS_TO_mS(1000);
#define ONE_KHZ_MICROS uS_TO_mS // 1000 uS for 1khz timer cycle for encoder

extern uint8_t selCH;
