#include <Arduino.h>
#include "TMOC_HW.h"
#include "DacESP32.h"
#include <CtrlPanel.h>
#include <bitHelpers.h>
#include <MagicButton.h>
#include <ClickEncoder.h>
#include "TuringRegister.h"
#include <OutputRegister.h>
#include <ESP32AnalogRead.h>
#include <ClickEncoderInterface.h>
#include "ESP32_New_TimerInterrupt.h"

ClickEncoder encoder(ENC_A, ENC_B, ENC_SW, ENC_STEPS_PER_NOTCH, ENC_ACTIVE_LOW);
ClickEncoderInterface encoderInterface(encoder, ENC_SENSITIVITY);

MagicButton writeHigh(TOGGLE_UP, 1, 0);
MagicButton writeLow(TOGGLE_DOWN, 1, 0);

ESP32AnalogRead cvA;        // "CV" input
ESP32AnalogRead cvB;        // "NOISE" input
ESP32AnalogRead ctrl;       // "LOOP" variable resistor

hw_timer_t *timer1(NULL);   // Timer library takes care of telling this where to point

DacESP32 voltagesExpander(static_cast<gpio_num_t>(DAC1_CV_OUT));

OutputRegister<uint8_t> triggers(SR_CLK, SR_DATA, TRIG_SR_CS, trgMap);
OutputRegister<uint16_t> leds(SR_CLK, SR_DATA, LED_SR_CS, regMap);
TuringRegister alan(&ctrl, &writeHigh, &writeLow);

uint8_t BIT_REGISTER(0);    // Blue LEDs + DAC8080
uint8_t FDR_REGISTER(0);    // Green Fader LEDs
uint8_t TRG_REGISTER(0);    // Gate/Trigger outputs + yellow LEDs

uint8_t PATTERN_START       [8]{0, 0, 0, 0, 0, 0, 0, 0};
uint8_t PATTERN_OFFSET      [8]{0, 0, 0, 0, 0, 0, 0, 0};
uint8_t PATTERN_LENGTH      [8]{8, 8, 8, 8, 8, 8, 8, 8};
uint8_t PROGRAM_COUNTER     [8]{0, 0, 0, 0, 0, 0, 0, 0};
uint32_t GATE_PATTERN       [8]{0, 0, 0, 0, 0, 0, 0, 0};

volatile bool newGate_FLAG  [NUM_GATES_IN]{0, 0};
volatile bool gateIn_VAL    [NUM_GATES_IN]{0, 0};
// volatile long rightNOW_micros(0);
// uint8_t SEL_STEP(0);

const float NEVER_FLIP(3.12);
const float ALWAYS_FLIP(0.26);


////////////////////////////
// ISR for Timer 1
bool FADERS_RUNNING(false);
void ICACHE_RAM_ATTR onTimer1()
{
  for (uint8_t fd = 0; fd < 8; ++fd)
  {
    if (!FADERS_RUNNING) continue;
    pFaders[fd]->service();
  }

  encoder.service();
  writeLow.service();
  writeHigh.service();

  for (uint8_t gate(0); gate < NUM_GATES_IN; ++gate)
  {
    bool val(!digitalRead(GATE_PIN[gate]));
    if (val && !gateIn_VAL[gate])
    {
      newGate_FLAG[gate] = 1;
    }
    gateIn_VAL[gate] = val;
  }

  if (triggers.clockExpired())
  {
    triggers.allOff();
  }
}


bool readFlag(uint8_t gate)
{
  bool ret(0);
  cli();
  if (newGate_FLAG[gate])
  {
    ret = 1;
    newGate_FLAG[gate] = 0;
  }
  sei();
  return ret;
}

bool CLOCK()
{
  return readFlag(0);
}

bool RESET()
{
  return readFlag(1);
}


// Spits out the binary representation of "val" to the serial monitor
void printBits(uint8_t  val){
  for (auto bit = 7; bit >= 0; --bit){ Serial.print(bitRead(val, bit) ? '1' : '0'); }
  Serial.println(' ');
}
void printBits(uint16_t val){
  for (auto bit = 15; bit >= 0; --bit){ Serial.print(bitRead(val, bit) ? '1' : '0'); }
  Serial.println(' ');
}


void setup()
{
  Serial.begin(115200);

  pinMode(SR_CLK,           OUTPUT);
  pinMode(SR_DATA,          OUTPUT);
  pinMode(LED_SR_CS,        OUTPUT);
  pinMode(TRIG_SR_CS,       OUTPUT);
  pinMode(DAC1_CV_OUT,      OUTPUT);
  for (uint8_t p(0); p < 4; ++p)
  {
    pinMode(PWM_OUT[p],     OUTPUT);
  }

  digitalWrite(LED_SR_CS,   HIGH);
  digitalWrite(TRIG_SR_CS,  HIGH);

  cvA.attach(CV_IN_A);
  cvB.attach(CV_IN_B);
	ctrl.attach(LOOP_CTRL);

  pinMode(CLOCK_IN,         INPUT_PULLUP);
  pinMode(RESET_IN,         INPUT_PULLUP);

  triggers.setReg(0);
  triggers.clock();

  initADC();

  timer1 = timerBegin(1, 80, true);
  timerAttachInterrupt(timer1, &onTimer1, true);

  timerAlarmWrite(timer1, ONE_KHZ_MICROS, true);
  timerAlarmEnable(timer1);

  initFaders();

  randomSeed(analogRead(UNUSED_ANALOG));
  alan.jam(random(1, 255) >> 8 | random(1, 255));
  uint8_t initReg(alan.getOutput());
  leds.setReg(~initReg, 0);
  leds.setReg(initReg, 1);
  leds.clock();

  FADERS_RUNNING = true;
}


void expandVoltages()
{
  float sum(0);
  for (int channel = 0 ; channel < NUM_FADERS; channel++)
  {
    if (!bitRead(BIT_REGISTER, channel))
    {
      continue;
    }
    sum += float(pFaders[chSliders[channel]]->read());
  }
  sum *= 3.134;
  voltagesExpander.outputVoltage(sum / float(pADC0->maxValue()));
}


void turingStep(int8_t steps)
{
  if (RESET())
  {
    BIT_REGISTER = BIT_REGISTER << PROGRAM_COUNTER[0] | BIT_REGISTER >> (15 - 8);
    for (uint8_t ch(0); ch < NUM_CHANNELS; ++ch)
    {
      PROGRAM_COUNTER[ch] = PATTERN_START[ch];
    }
    alan.jam(BIT_REGISTER);
  }
  else
  {
    alan.iterate(steps);
  }

  TRG_REGISTER = alan.getOutput();
  triggers.setReg(TRG_REGISTER);
  triggers.clock();
  expandVoltages();

  BIT_REGISTER = TRG_REGISTER;
  leds.setReg(BIT_REGISTER, 1);
  FDR_REGISTER = ~BIT_REGISTER;
  leds.setReg(FDR_REGISTER, 0);
  leds.clock();
}


void loop()
{
  if (CLOCK())
  {
    turingStep(1);
    Serial.println("clock");
  }

  // Check for new events from our encoder and clear the "ready" flag if there are
  encEvnts evt(encoderInterface.getEvent());
  switch(evt)
  {
    case encEvnts :: Click:
      break;
    case encEvnts :: DblClick:
      break;
    case encEvnts :: Left:
      turingStep(-1);
      break;
    case encEvnts :: ShiftLeft:
      break;
    case encEvnts :: Right:
      turingStep(1);
      break;
    case encEvnts :: ShiftRight:
      break;
    case encEvnts :: Press:
      Serial.println("PRESS");
      break;
    case encEvnts :: ClickHold:
      Serial.println("CLICK AND HOLD");
      break;
    case encEvnts :: Hold:
      Serial.println("HOLD");
      break;
    case encEvnts :: NUM_ENC_EVNTS:
    default:
      break;
  }
}
