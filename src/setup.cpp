#include "setup.h"
#include "hwio.h"
#include "timers.h"

float ONE_OVER_ADC_MAX(0.0);

void setupESP32_ADCs()
{
  // Set up internal ADCs
  cvA.attach(CV_IN_A);
  cvB.attach(CV_IN_B);
  turing.attach(LOOP_CTRL);
}

// Sequencer state variables
uint8_t OctaveRange(1);
uint8_t currentBank(0);

int8_t patternPending(-1);

bool set_bit_0(0);
bool clear_bit_0(0);

ModeControl mode;

// Core Shift Register functionality
Stochasticizer stoch(turing, cvA);
TuringRegister alan(stoch);

// Don't light up "locked" faders regardless of bit vals
uint8_t faderLockStateReg(0xFF);
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
    setTrigRegister(0);
    triggers.clock();

    // Set up external 8 channel ADC
    initADC();
    ONE_OVER_ADC_MAX = 1.0f / faderBank[0]->getMax();

    // Set up DAC
    output.init();

    for (uint8_t fd = 0; fd < NUM_FADERS; ++fd)
    {
      // Don't light up locked faders
      faderBank[fd]->setDefaults();
      bitWrite(faderLockStateReg, fd, faderBank[fd]->getLockState() == STATE_UNLOCKED);
    }

    uint16_t randomReg((random(1, 255) << 8) | random(1, 255));
    for (int8_t bk(NUM_BANKS - 1); bk >= 0; --bk)
    {
      alan.writeToRegister(~(0x01 << bk), bk);
      for (uint8_t fd = 0; fd < NUM_FADERS; ++fd)
      {
        faderBank[fd]->saveActiveCtrl(bk);
      }
    }

    setupTimers();

    // Set pattern LEDs to display current pattern
    setDacRegister(alan.getOutput());
    setFaderRegister(dacRegister & faderLockStateReg);
    leds.clock();
}