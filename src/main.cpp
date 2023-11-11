#include <Arduino.h>
// #include <pwmWrite.h>
#include "TMOC_HW.h"
#include "DacESP32.h"
#include <CtrlPanel.h>
#include <bitHelpers.h>
#include <ClickEncoder.h>
#include <OutputRegister.h>
#include <ESP32AnalogRead.h>
#include <ClickEncoderInterface.h>

/*
  // setting all pins at the same time to either HIGH or LOW
  sr.setAllHigh(); // set all pins HIGH
  sr.setAllLow();  // set all pins LOW
  sr.set(i, HIGH); // set single pin HIGH

  // set all pins at once
  uint8_t pinValues[] = { B10101010 };
  sr.setAll(pinValues);

  // read pin (zero based, i.e. 6th pin)
  uint8_t stateOfPin5 = sr.get(5);
  sr.set(6, stateOfPin5);

  // set pins without immediate update
  sr.setNoUpdate(0, HIGH);
  sr.setNoUpdate(1, LOW);
  sr.updateRegisters(); // update the pins to the set values
  */

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

uint8_t BIT_REGISTER(0);    // Blue LEDs + DAC8080
uint8_t FDR_REGISTER(0);    // Green Fader LEDs
uint8_t TRG_REGISTER(0);    // Gate/Trigger outputs + yellow LEDs

volatile bool newGate_FLAG [NUM_GATES_IN]{0, 0};
volatile bool gateIn_VAL   [NUM_GATES_IN]{0, 0};
volatile long rightNOW_micros(0);

uint8_t PROGRAM_COUNTER(0);

bool count(0);
uint8_t acc(0);
uint8_t acc2(0);
bool FADERS_RUNNING(false);

////////////////////////////
// ISR for Timer 1
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
    if (val == gateIn_VAL[gate])
    {
      continue;
    }
    gateIn_VAL[gate] = val;
    if (val)
    {
      newGate_FLAG[gate] = 1;
    }
  }
}


bool getGate(uint8_t gate)
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

bool getClock()
{
  if (!getGate(0));
  {
    return 0;
  }

  return 1;
}

bool getReset()
{
  if (!getGate(1))
  {
    return 0;
  }

  PROGRAM_COUNTER = 0;
  return 1;
}


// Probability of returning true increases as "prob" approaches 255
bool coinToss(uint8_t prob)
{
  return (rand() % 255 < prob);
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
  randomSeed(analogRead(UNUSED_ANALOG));
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

  initADC();

  timer1 = timerBegin(1, 80, true);
  timerAttachInterrupt(timer1, &onTimer1, true);

  timerAlarmWrite(timer1, ONE_KHZ_MICROS, true);
  timerAlarmEnable(timer1);

  initFaders();
  FADERS_RUNNING = true;
  triggers.setReg(1);
  leds.setReg(1, 0);
  uint8_t temp(0xFF);
  bitWrite(temp, 0, 0);
  leds.setReg(1, 1);
  leds.clock();
  triggers.clock();
}

void loop()
{
  getReset();
  getClock();
  cvA.readVoltage();
  cvB.readVoltage();
  ctrl.readVoltage();
  voltagesExpander.outputVoltage(static_cast<float>(255));

  uint16_t val[16];
  for (int channel = 0 ; channel < NUM_FADERS; channel++)
  {
    val[channel] = pFaders[chSliders[channel]]->read();
  }

  // Check for new events from our encoder and clear the "ready" flag if there are
  encEvnts evt(encoderInterface.getEvent());
  switch(evt)
  {
    case encEvnts :: Click:
      triggers.setReg(~uint8_t(triggers.D()));
      triggers.clock();
      break;
    case encEvnts :: DblClick:
      leds.setReg(~uint8_t(leds.D() >> 8), 1);
      leds.clock();
      break;
    case encEvnts :: Left:
      triggers.rotateRight(1);
      triggers.clock();
      break;
    case encEvnts :: ShiftLeft:
      leds.rotateRight(1, 1);
      leds.clock();
      break;
    case encEvnts :: Right:
      triggers.rotateRight(-1);
      triggers.clock();
      break;
    case encEvnts :: ShiftRight:
      leds.rotateRight(-1, 1);
      leds.clock();
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

  ButtonState lo(writeLow.readAndFree());
  switch(lo)
  {
    case ButtonState :: Clicked:
    case ButtonState :: ClickedAndHeld:
      leds.rotateRight(-1, 0);
      leds.clock();
      break;
    case ButtonState :: DoubleClicked:
      Serial.println("DOWN DOWN");
      break;
    default:
      break;
  }

  ButtonState hi(writeHigh.readAndFree());
  switch(hi)
  {
    case ButtonState :: Clicked:
    case ButtonState :: ClickedAndHeld:
      leds.rotateRight(1, 0);
      leds.clock();
      break;
    case ButtonState :: DoubleClicked:
      Serial.println("UP UP");
      break;
    default:
      break;
  }

  // if (!count)
  // {
  //   mapOutputRegister(acc, trgLeds, TRG_REGISTER);
  // }
  // else
  // {
  //   mapOutputRegister(~acc, trgLeds, TRG_REGISTER);
  // }

  // if (acc2 % 2)
  // {
  //   mapOutputRegister(~(1 << (acc % 8)), bitLeds, BIT_REGISTER);
  //   mapOutputRegister(1 << (7 - (acc % 8)), fdrLeds, FDR_REGISTER);
  // }
  // else
  // {
  //   mapOutputRegister(1 << (acc % 8), bitLeds, BIT_REGISTER);
  //   mapOutputRegister(1 << (acc % 8), fdrLeds, FDR_REGISTER);
  // }

  // digitalWrite(LED_SR_CS, LOW);
  // leds.write(FDR_REGISTER);
  // leds.write(BIT_REGISTER);
  // digitalWrite(LED_SR_CS, HIGH);
}
