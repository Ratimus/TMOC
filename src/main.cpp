#include <Arduino.h>
#include <Wire.h>
#include "TMOC_HW.h"
#include "DacESP32.h"
#include <CtrlPanel.h>
#include <bitHelpers.h>
#include <MagicButton.h>
#include <ClickEncoder.h>
#include "TuringRegister.h"
#include <OutputRegister.h>
#include <ESP32AnalogRead.h>
#include <Adafruit_MCP4728.h>
#include <ClickEncoderInterface.h>
#include "ESP32_New_TimerInterrupt.h"

Adafruit_MCP4728 MCP4728;

MagicButton writeHigh(TOGGLE_UP, 1, 1);
MagicButton writeLow(TOGGLE_DOWN, 1, 1);

ClickEncoder encoder(ENC_A, ENC_B, ENC_SW, ENC_STEPS_PER_NOTCH, ENC_ACTIVE_LOW);
ClickEncoderInterface encoderInterface(encoder, ENC_SENSITIVITY);

// Gate Inputs:
//  - Clock
//  - [was Trig out]
// CV Inputs:
//  - CV
//  - [was Seq out]
//  - [was Noise out]
//  - [was Voltages Seq out]
//  - [was Voltages Inverted out]
// Controls:
//  - [was Scale]
//  - Turing
//  - [was Voltages Scale]
//  - [was Voltages Offset]

ESP32AnalogRead cvA;        // "CV" input
ESP32AnalogRead cvB;        // "NOISE" input
ESP32AnalogRead turing;     // "LOOP" variable resistor


uint8_t currentBank(0);
bool faderInitValsSet[NUM_BANKS];

bool pendingMove(0);
bool pendingChange(0);

hw_timer_t *timer1(NULL);   // Timer library takes care of telling this where to point

DacESP32 voltsExp(static_cast<gpio_num_t>(DAC1_CV_OUT));

TuringRegister alan(&ctrl);

OutputRegister<uint16_t>  leds(SR_CLK, SR_DATA, LED_SR_CS, regMap);
OutputRegister<uint8_t>   triggers(SR_CLK, SR_DATA, TRIG_SR_CS, trgMap);

uint8_t dacRegister(0);    // Blue LEDs + DAC8080
uint8_t fdrRegister(0);    // Green Fader LEDs
uint8_t trgRegister(0);    // Gate/Trigger outputs + yellow LEDs
uint8_t faderLockStateReg(0);
bool    faderLocksChanged(1);

ScaleType currentScale(harmMinor);
volatile bool gateInVals    [NUM_GATES_IN]{0, 0};
volatile bool newGateFlags  [NUM_GATES_IN]{0, 0};

const MCP4728_channel_t DAC_CH[]{MCP4728_CHANNEL_D, MCP4728_CHANNEL_B, MCP4728_CHANNEL_A, MCP4728_CHANNEL_C};
bool  fadersRunning(false);
float ONE_OVER_ADC_MAX(0.0);

// Note: you're still gonna need to clock this before it updates
void setDacRegister(uint8_t val)  { dacRegister = val; leds.setReg(dacRegister, 1);  }
void setTrigRegister(uint8_t val) { trgRegister = val; triggers.setReg(trgRegister); }
void setFaderRegister(uint8_t val){ fdrRegister = val; leds.setReg(fdrRegister, 0);  }

uint16_t quantize(uint16_t note)
{
  if (note > 96)
  {
    return 96;
  }

  if (currentScale == chromatic)
  {
    Serial.println("chromatic");
    return note;
  }

  if (currentScale == wholeTone)
  {
    Serial.println("whole tone");
    return (note / 2) * 2;
  }

  const uint8_t *pScale = *(SCALES + (uint8_t)currentScale);
  uint16_t octave(note / 12);
  uint16_t semiTone(*(pScale + note % 12));
  return octave * 12 + semiTone;
}

////////////////////////////
// ISR for Timer 1
void ICACHE_RAM_ATTR onTimer1()
{
  if (fadersRunning)
  {
    for (uint8_t fd = 0; fd < 8; ++fd)
    {
      pFaders[fd]->service();
    }
  }

  encoder.service();
  writeLow.service();
  writeHigh.service();

  for (uint8_t gate(0); gate < NUM_GATES_IN; ++gate)
  {
    bool val(!digitalRead(GATE_PIN[gate]));
    if (val && !gateInVals[gate])
    {
      newGateFlags[gate] = 1;
    }
    gateInVals[gate] = val;
  }

  if (triggers.clockExpired())
  {
    triggers.allOff(true);
  }
}


// Thread-safe getter for gate input rising edge flags
bool readFlag(uint8_t gate)
{
  bool ret(0);
  cli();
  if (newGateFlags[gate])
  {
    ret = 1;
    newGateFlags[gate] = 0;
  }
  sei();

  if (ret)
  {
    if (gate)
    {
      Serial.printf("RESET: %s\n", ret ? "TRUE" : "FALSE");
    }
    else
    {
      Serial.printf("CLOCK: %s\n", ret ? "TRUE" : "FALSE");
    }
  }
  return ret;
}

bool checkClockFlag(){ return readFlag(0); }
bool checkResetFlag(){ return readFlag(1); }


// Spits out the binary representation of "val" to the serial monitor
void printBits(uint8_t  val)
{
  for (auto bit = 7; bit >= 0; --bit)
  {
    Serial.print(bitRead(val, bit) ? '1' : '0');
  }
  Serial.println(' ');
}
void printBits(uint16_t val)
{
  for (auto bit = 15; bit >= 0; --bit)
  {
    Serial.print(bitRead(val, bit) ? '1' : '0');
  }
  Serial.println(' ');
}

void calibrate();
bool ON_BATTERY_POWER(0);
void setup()
{
  // Serial.begin(115200);

  pinMode(LED_SR_CS,        OUTPUT);
  pinMode(TRIG_SR_CS,       OUTPUT);
  for (uint8_t p(0); p < 4; ++p)
  {
    pinMode(PWM_OUT[p],     OUTPUT);
  }

  // 74HC595 Latch pin is active low
  digitalWrite(LED_SR_CS,   HIGH);
  digitalWrite(TRIG_SR_CS,  HIGH);

  cvA.attach(CV_IN_A);
  cvB.attach(CV_IN_B);
  ctrl.attach(LOOP_CTRL);

  pinMode(CLOCK_IN,         INPUT);
  pinMode(RESET_IN,         INPUT);

  setTrigRegister(0);
  triggers.clock();

  initADC();
  ONE_OVER_ADC_MAX = 1.0f / pADC0->maxValue();

  Serial.println("MCP4728 test...");
  if (!MCP4728.begin(0x64))
  {
    Serial.println("Failed to find MCP4728 chip");
    while (1) { delay(1); }
  }
  Serial.println("MCP4728 chip initialized");

  for (uint8_t ch(0); ch < 4; ++ch)
  {
    MCP4728.setChannelValue((MCP4728_channel_t)ch, 0, MCP4728_VREF_INTERNAL);
  }

  for (uint8_t fd(0); fd < NUM_BANKS; ++fd)
  {
    faderInitValsSet[fd] = false;
  }

  timer1 = timerBegin(1, 80, true);
  timerAttachInterrupt(timer1, &onTimer1, true);
  timerAlarmWrite(timer1, ONE_KHZ_MICROS, true);
  timerAlarmEnable(timer1);

  initFaders(fadersRunning);
  delay(100);
  for (uint8_t fd = 0; fd < NUM_FADERS; ++fd)
  {
    (pCV + fd)->select(0);
    (pCV + fd)->overWrite();
    bitWrite(faderLockStateReg, fd, (pCV + fd)->pACTIVE->getLockState() != STATE_UNLOCKED);
    Serial.printf("CH %u: %u\n", fd, (pCV + fd)->readActiveCtrl());
  }
  faderInitValsSet[0] = true;
  // calibrate();

  randomSeed(analogRead(UNUSED_ANALOG));
  uint16_t randomReg((random(1, 255) << 8) | random(1, 255));
  for (uint8_t bk(0); bk < NUM_BANKS; ++bk)
  {
    alan.preFill(randomReg, bk);
  }

  uint8_t initReg(alan.getOutput());
  setDacRegister(initReg);
  setFaderRegister(initReg | faderLockStateReg);
  leds.clock();
  Serial.printf("%sON BATTERY POWER\n", ON_BATTERY_POWER ? "" : "NOT ");
}


void writeRawValToDac(uint8_t ch, uint16_t val)
{
  MCP4728.setChannelValue(DAC_CH[ch], val, MCP4728_VREF_INTERNAL);
}


uint16_t noteToDacVal(uint8_t ch, uint16_t note)
{
  const uint16_t *calTable(DAC_CAL_TABLES[DAC_CH[ch]]);
  uint8_t octave(note / 12);
  if (octave > CAL_TABLE_HIGH_OCTAVE)
  {
    octave = CAL_TABLE_HIGH_OCTAVE;
  }

  uint16_t semiTone(note - (octave * 12));
  uint16_t inc(0);
  if (octave < CAL_TABLE_HIGH_OCTAVE)
  {
    inc = calTable[octave + 1] - calTable[octave];
  }
  else
  {
    inc = calTable[CAL_TABLE_HIGH_OCTAVE] - calTable[CAL_TABLE_HIGH_OCTAVE - 1];
  }

  inc /= 12;
  uint16_t setVal(calTable[octave] + inc * semiTone);
  if (setVal > 4095)
  {
    return 4095;
  }

  return setVal;
}


void writeNoteToDac(uint8_t ch, uint16_t note)
{
  writeRawValToDac(ch, noteToDacVal(ch, note));
}


void getStepIndices(const uint8_t reg, uint8_t (*indices)[4])
{
  *indices[0] = 0;
  bool s0(bitRead(reg, 0) ^ !(bitRead(reg, 1) ^ bitRead(reg, 2)));
  bool s1(bitRead(reg, 3) ^ !(bitRead(reg, 4) ^ bitRead(reg, 5)));
  bool s2(bitRead(reg, 6) ^ !(bitRead(reg, 7) ^ bitRead(reg, 0)));
  bitWrite(*indices[0], 0, s0);
  bitWrite(*indices[0], 1, s1);
  bitWrite(*indices[0], 2, s2);

  *indices[1] = 0;
  s0 = (bitRead(reg, 7) ^ !(bitRead(reg, 4) ^ bitRead(reg, 1)));
  s1 = (bitRead(reg, 6) ^ !(bitRead(reg, 3) ^ bitRead(reg, 0)));
  s2 = (bitRead(reg, 5) ^ !(bitRead(reg, 2) ^ bitRead(reg, 7)));
  bitWrite(*indices[1], 0, s0);
  bitWrite(*indices[1], 1, s1);
  bitWrite(*indices[1], 2, s2);

  *indices[2] = 0;
  s0 = (bitRead(reg, 3) ^ !(bitRead(reg, 6) ^ bitRead(reg, 1)));
  s1 = (bitRead(reg, 0) ^ !(bitRead(reg, 2) ^ bitRead(reg, 5)));
  s2 = (bitRead(reg, 5) ^ !(bitRead(reg, 7) ^ bitRead(reg, 1)));
  bitWrite(*indices[2], 0, s0);
  bitWrite(*indices[2], 1, s1);
  bitWrite(*indices[2], 2, s2);

  *indices[3] = 0;
  s0 = (bitRead(reg, 0) ^ !(bitRead(reg, 6) ^ bitRead(reg, 5)));
  s1 = (bitRead(reg, 2) ^ !(bitRead(reg, 1) ^ bitRead(reg, 7)));
  s2 = (bitRead(reg, 4) ^ !(bitRead(reg, 3) ^ bitRead(reg, 1)));
  bitWrite(*indices[3], 0, s0);
  bitWrite(*indices[3], 1, s1);
  bitWrite(*indices[3], 2, s2);
}


void expandVoltages()
{
  uint8_t stepIndices[4];
  getStepIndices(dacRegister, &stepIndices);

  uint16_t faderVals[8];
  uint16_t noteVals[4]{0, 0, 0, 0};
  for (uint8_t ch(0); ch < NUM_FADERS; ++ch)
  {
    faderVals[ch] = (pCV + ch)->readActiveCtrl();
    if (bitRead(dacRegister, ch))
    {
      noteVals[0] += faderVals[ch];
    }
    else
    {
      noteVals[2] += faderVals[ch];
    }
  }

  noteVals[1] = faderVals[stepIndices[0]] + faderVals[stepIndices[1]];
  noteVals[3] = faderVals[stepIndices[2]] + faderVals[stepIndices[3]];

  float sum((float)noteVals[0]);
  sum *= ONE_OVER_ADC_MAX;
  if (sum > 1.0f)
  {
    sum = 1.0f;
  }
  voltsExp.outputVoltage(sum * 3.3f);

  for (uint8_t ch(0); ch < 4; ++ch)
  {
    writeNoteToDac(ch, quantize(noteVals[ch]));
  }
}


uint8_t calcTriggerVals(uint8_t inputReg)
{
  // Determine how far through the loop we are
  uint8_t percent(map(alan.getStep(), 0, alan.getLength() + 1, 0, 256));

  // Reverse out progress val and XOR it with our input val
  uint8_t outputReg(bitReverse(percent) ^ inputReg);

  // Preserve bit0 from the input because it's special
  bitWrite(outputReg, 0, bitRead(inputReg, 0));
  return outputReg;
}


void turingStep(int8_t steps)
{
  // Turn off the triggers in case a new clock came in before they expired
  triggers.allOff(true);
  Serial.printf("----------------------------\n");
  if (pendingMove || pendingChange)
  {
    if (pendingMove)
    {
      alan.moveToPattern(currentBank);
      pendingMove = false;
    }
    else
    {
      alan.overWritePattern(currentBank);
      pendingChange = false;
    }

    for (uint8_t fd = 0; fd < NUM_FADERS; ++fd)
    {
      (pCV + fd)->select(currentBank);
      if (!faderInitValsSet[currentBank])
      {
        (pCV + fd)->overWrite();
        faderInitValsSet[currentBank] = true;
      }
      Serial.printf("control %d: %p\n", fd, (pCV + fd)->pACTIVE);
      Serial.printf("val: %u\n", (pCV + fd)->readActiveCtrl());
      bitWrite(faderLockStateReg, fd, (pCV + fd)->pACTIVE->getLockState() != LockState :: STATE_UNLOCKED);
    }

    Serial.printf("------ select track %d ------\n", currentBank);
    faderLocksChanged = true;
  }

  if (checkResetFlag())
  {
    alan.reset();
  }

  bool downBeat(alan.iterate(steps));

  uint8_t tmpReg(alan.getOutput());
  setDacRegister(tmpReg);
  setFaderRegister(dacRegister | faderLockStateReg);
  uint8_t trigReg(calcTriggerVals(dacRegister));

  // Trig 0 is the down beat
  bitWrite(trigReg, 7, downBeat);
  setTrigRegister(trigReg);
  Serial.print("output:  ");
  printBits(trigReg);

  // Update voltages first so they are ready when the triggers update
  expandVoltages();

  leds.clock();
  triggers.clock();
}


void handleEncoder()
{
  // Check for new events from our encoder and clear the "ready" flag if there are
  switch(encoderInterface.getEvent())
  {
    case encEvnts :: DblClick:
      alan.lengthMINUS();
      Serial.printf("LENGTH: %u\n", alan.getLength());
      break;

    case encEvnts :: Click:
      alan.lengthPLUS();
      Serial.printf("LENGTH: %u\n", alan.getLength());
      break;

    case encEvnts :: Hold:
      alan.reset();
      Serial.println("reset");
      break;

    case encEvnts :: Right:
    case encEvnts :: Left:
    case encEvnts :: Press:
      break;


    case encEvnts :: ClickHold:
      currentScale = ScaleType((int)currentScale + 1);
      if (currentScale == numScales)
      {
        currentScale = ScaleType(0);
      }
      break;

    case encEvnts :: ShiftLeft:
      if (ON_BATTERY_POWER) turingStep(-1);
      break;

    case encEvnts :: ShiftRight:
      if (ON_BATTERY_POWER) turingStep(1);
      break;

    case encEvnts :: NUM_ENC_EVNTS:
    default:
      break;
  }
}


void ReadCtrls()
{
  static bool initialized(0);
  if (!initialized)
  {
    initialized = true;
    for (uint8_t fd = 0; fd < NUM_FADERS; ++fd)
    {
      (pCV + fd)->select(currentBank);
      Serial.printf("control %d: %p\n", fd, (pCV + fd)->pACTIVE);
    }
  }
}


void loop()
{
  if (checkClockFlag())
  {
    int8_t step(1);
    uint16_t cvbVal(cvB.readRaw());
    Serial.printf("Loop: %u\n", ctrl.readRaw());
    Serial.printf("CV_A: %u\n", cvA.readRaw());
    Serial.printf("CV_B: %u\n", cvbVal);
    if (cvbVal > 2047)
    {
      step = -1;
    }
    turingStep(step);
  }

  ButtonState clickies[]{ writeLow.readAndFree(), writeHigh.readAndFree() };
  // Click Down:        store current pattern @ n and move to n-1
  // Click Up:          store current pattern @ n and move to n+1
  // DoubleClick Up:    store current pattern @ n+1 and "move" to n+1
  // DoubleClick Down:  store current pattern @ n-1 and "move" to n-1
  if (clickies[0] == ButtonState :: Clicked || clickies[1] == ButtonState :: Clicked)
  {
    pendingMove   = true;
    pendingChange = false;
    if (clickies[1] == ButtonState :: Clicked)
    {
      ++currentBank;
      currentBank %= NUM_BANKS;
    }
    else
    {
      if (currentBank == 0)
      {
        currentBank = NUM_BANKS - 1;
      }
      else
      {
        --currentBank;
      }
    }
  }
  else if (clickies[0] == ButtonState :: DoubleClicked || clickies[1] == ButtonState :: DoubleClicked)
  {
    pendingMove   = false;
    pendingChange = true;
    uint8_t next(currentBank);
    if (clickies[1] == ButtonState :: DoubleClicked)
    {
      ++currentBank;
      currentBank %= NUM_BANKS;
    }
    else
    {
      if (next == 0)
      {
        next = NUM_BANKS - 1;
      }
      else
      {
        --next;
      }
    }

    currentBank = next;
  }

  if (faderLocksChanged)
  {
    for (uint8_t fd(0); fd < NUM_FADERS; ++fd)
    {

      Serial.printf("control %d: %p\n", fd, (pCV + fd)->pACTIVE);
      Serial.printf("val: %u\n", (pCV + fd)->readActiveCtrl());
    }

    faderLocksChanged = false;
  }

  for (uint8_t fd = 0; fd < NUM_FADERS; ++fd)
  {
    (pCV + fd)->readActiveCtrl();
    bitWrite(faderLockStateReg, fd, (pCV + fd)->pACTIVE->getLockState() != LockState :: STATE_UNLOCKED);
    leds.tempWrite(dacRegister | faderLockStateReg);
  }

  handleEncoder();
}

/*
create a bank of 8 registers
doubleclick up:
  write current register to current storage slot in bank
click up:
  move to next register in bank ON STEP 0
click down:
  move to previous register in bank ON STEP 0

what is stored:
  - shift register contents
  - fader positions
  - 4x VEX indices

on transition:
  change VEX indices, lock the faders, and use the old values
  - when we switch to a different bank, the faders lock on their *current* positions
  so register contents and VEX indices are stored with the bank, but fader positions are
  only loosely coupled to the bank

what we need:
- bank of n TuringRegisters
- index to store which register we are currently using
- counter to store total number of active registers in our bank
- control panel of locking faders with n scenes
  - indices for 4xVEX are derived from dacRegister, so we don't need to store them
  - fader values are stored in locking faders, so we don't need to store those, either
*/
void calibrate()
{
  uint8_t selch(0);
  uint8_t selreg(1);
  leds.tempWrite(1, 0);
  leds.tempWrite(1, 1);

  uint16_t outval(0);
  (pCV + selch)->select(0);
  Serial.printf("ch: %u\n", selch);
  while (true)
  {
    if (writeHigh.readAndFree())
    {
      writeRawValToDac(selch, 0);
      ++selch;
      if (selch == 4) selch = 0;
      selreg = 0 | (1 << selch);
      (pCV + selch)->select(0);
      leds.tempWrite(selreg, 0);
      leds.tempWrite(selreg, 1);
      Serial.printf("ch: %u\n", selch);
    }

    outval = 0;
    for (uint8_t bn(0); bn < 8; ++bn)
    {
      outval += pFaders[sliderMap[bn]]->read() >> (1 + bn);
    }
    if (outval > 4095)
    {
      outval = 4095;
    }
    MCP4728.setChannelValue((MCP4728_channel_t)selch, outval, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
    if (writeLow.readAndFree())
    {
      Serial.printf("%u\n", outval);
    }
  }
}