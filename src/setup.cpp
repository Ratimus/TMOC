#include <Arduino.h>
#include "setup.h"
#include "hwio.h"
#include "timers.h"
#include <Wire.h>
#include "hw_constants.h"
#include <GateIn.h>
#include <RatFuncs.h>
#include "OutputDac.h"
#include <bitHelpers.h>
#include <ClickEncoder.h>
#include <OutputRegister.h>
#include "ESP32_New_TimerInterrupt.h"


void setupESP32_ADCs()
{
  // Set up internal ADCs
  cvA.attach(CV_IN_A);
  cvB.attach(CV_IN_B);
  turing.attach(LOOP_CTRL);
}

// Sequencer state variables
uint8_t OctaveRange(1);
ModeControl mode;

// Core Shift Register functionality
Stochasticizer stoch(turing, cvA);
TuringRegister alan(stoch);

const uint8_t sliderMapping[]{7, 6, 5, 4, 3, 2, 1, 0};
ControllerBank  faders(NUM_FADERS, NUM_BANKS, sliderMapping);

bool    faderLocksChanged(1);


void setThingsUp()
{
  #ifdef RATDEBUG
    Serial.begin(115200);
    delay(100);
  #endif

  // Shift register CS pins
  pinMode(LED_SR_CS,        OUTPUT);
  pinMode(TRIG_SR_CS,       OUTPUT);

  // PWM outputs
  for (uint8_t p(0); p < 4; ++p)
  {
    pinMode(PWM_OUT[p],     OUTPUT);
  }

  // 74HC595 Latch pins is active low
  digitalWrite(LED_SR_CS,   HIGH);
  digitalWrite(TRIG_SR_CS,  HIGH);

  // Set up internal ADCs
  setupESP32_ADCs();

  // Clear trigger output register
  triggers.reset();

  // Set up external 8 channel ADC
  dbprintf("Here comes the ADCs\n");

  faders.init(SPI_DATA_OUT, SPI_DATA_IN, SPI_CLK, ADC0_CS);
  dbprintf("Yup, there they are\n");

  // Set up DAC
  output.init();

  setupTimers();
  alan.reset();

  // Set pattern LEDs to display current pattern
  panelLeds.updateAll();
}