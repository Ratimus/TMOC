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

// Set TRUE to enable Serial printing (and whatever else)
bool CASSIDEBUG(0);
#define print if (CASSIDEBUG) Serial.print
#define printf if (CASSIDEBUG) Serial.printf
#define println if (CASSIDEBUG) Serial.println

Adafruit_MCP4728 MCP4728;

MagicButton writeHigh(TOGGLE_UP, 1, 1);
MagicButton writeLow(TOGGLE_DOWN, 1, 1);

ClickEncoder encoder(ENC_A,
                     ENC_B,
                     ENC_SW,
                     ENC_STEPS_PER_NOTCH,
                     ENC_ACTIVE_LOW);

ClickEncoderInterface encoderInterface(encoder,
                                       ENC_SENSITIVITY);

// Gate Inputs:
//  - Clock
//  - Reset [was Trig out]
// CV Inputs:
//  - CV
//  - [was Seq out]
//  - [was Noise out]
//  - [was Voltages Seq out]
//  - [was Voltages Inverted out]
// Controls:
//  - Mode [was Scale]
//  - Turing

ESP32AnalogRead cvA;        // "CV" input
ESP32AnalogRead cvB;        // "NOISE" input
ESP32AnalogRead turing;     // "LOOP" variable resistor

uint8_t OctaveRange(1);
uint8_t currentBank(0);

// Indicates whether the current fader bank has been filled with real data
bool faderInitValsSet[NUM_BANKS];

bool shift(0);

bool set_bit_0(0);
bool clear_bit_0(0);

hw_timer_t *timer1(NULL);   // Timer library takes care of telling this where to point

// Here's the ESP32 DAC output
DacESP32 voltsExp(static_cast<gpio_num_t>(DAC1_CV_OUT));

// Core Shift Register functionality
TuringRegister alan(&turing, &cvA);

// Hardware interfaces for 74HC595s
OutputRegister<uint16_t>  leds(SR_CLK, SR_DATA, LED_SR_CS, regMap);
OutputRegister<uint8_t>   triggers(SR_CLK, SR_DATA, TRIG_SR_CS, trgMap);

// Data values for 74HC595s
uint8_t dacRegister(0);    // Blue LEDs + DAC8080
uint8_t fdrRegister(0);    // Green Fader LEDs
uint8_t trgRegister(0);    // Gate/Trigger outputs + yellow LEDs

// Don't light up "locked" faders regardless of bit vals
uint8_t faderLockStateReg(0xFF);
bool    faderLocksChanged(1);

// Keep track of Clock and Reset digital inputs
volatile bool   gateInVals    [NUM_GATES_IN]{0, 0};
volatile int8_t newGateFlags  [NUM_GATES_IN]{0, 0};

const MCP4728_channel_t DAC_CH[]{MCP4728_CHANNEL_D, MCP4728_CHANNEL_B, MCP4728_CHANNEL_A, MCP4728_CHANNEL_C};

// Don't service faders in ISR until they're fully initialized
bool  fadersRunning(false);
float ONE_OVER_ADC_MAX(0.0);

// Note: you're still gonna need to clock this before it updates
void setDacRegister(uint8_t val)  { dacRegister = val; leds.setReg(dacRegister, 1);  }
void setTrigRegister(uint8_t val) { trgRegister = val; triggers.setReg(trgRegister); }
void setFaderRegister(uint8_t val){ fdrRegister = val; leds.setReg(fdrRegister, 0);  }

void calibrate();

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
    if (val != gateInVals[gate])
    {
      newGateFlags[gate] = (int8_t)val - (int8_t)gateInVals[gate];
    }
    gateInVals[gate] = val;
  }
}


// Thread-safe getter for gate input rising edge flags
int8_t readFlag(uint8_t gate)
{
  int8_t ret;
  cli();
  ret = newGateFlags[gate];
  newGateFlags[gate] = 0;
  sei();

  return ret;
}

int8_t checkClockFlag(){ return readFlag(0); }
int8_t checkResetFlag(){ return readFlag(1) == 1; }


// Spits out the binary representation of "val" to the serial monitor
void printBits(uint8_t  val)
{
  for (auto bit = 0; bit < 8; ++bit)
  {
    print(bitRead(val, bit) ? '1' : '0');
  }
  println(' ');
}


void printBits(uint16_t val)
{
  for (auto bit = 15; bit >= 0; --bit)
  {
    print(bitRead(val, bit) ? '1' : '0');
  }
  println(' ');
}


void setup()
{
  if (CASSIDEBUG) Serial.begin(115200);
  delay(100);

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
  turing.attach(LOOP_CTRL);

  pinMode(CLOCK_IN,         INPUT);
  pinMode(RESET_IN,         INPUT);

  setTrigRegister(0);
  triggers.clock();

  initADC();
  ONE_OVER_ADC_MAX = 1.0f / pADC0->maxValue();

  println("MCP4728 test...");
  if (!MCP4728.begin(0x64))
  {
    println("Failed to find MCP4728 chip");
    while (1) { delay(1); }
  }
  println("MCP4728 chip initialized");

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

    // Don't light up locked faders
    bitWrite(faderLockStateReg, fd, (pCV + fd)->pACTIVE->getLockState() == STATE_UNLOCKED);
    printf("CH %u: %u\n", fd, (pCV + fd)->readActiveCtrl());
  }
  faderInitValsSet[0] = true;
  // calibrate();

  uint16_t randomReg((random(1, 255) << 8) | random(1, 255));
  for (uint8_t bk(0); bk < NUM_BANKS; ++bk)
  {
    alan.preFill(randomReg, bk);
  }

  setDacRegister(alan.getOutput());
  setFaderRegister(dacRegister & faderLockStateReg);
  leds.clock();
}


void writeRawValToDac(uint8_t ch, uint16_t val)
{
  MCP4728.setChannelValue(DAC_CH[ch], val, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
}


// Translates note values to raw DAC outputs using calibration tables
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


// void getStepIndices(const uint8_t reg, uint8_t *indices)
// {
//   *(indices) = 0;
//   bool s0(bitRead(reg, 0) ^ !(bitRead(reg, 1) ^ bitRead(reg, 2)));
//   bool s1(bitRead(reg, 3) ^ !(bitRead(reg, 4) ^ bitRead(reg, 5)));
//   bool s2(bitRead(reg, 6) ^ !(bitRead(reg, 7) ^ bitRead(reg, 0)));
//   bitWrite(*(indices + 0), 0, s0);
//   bitWrite(*(indices + 0), 1, s1);
//   bitWrite(*(indices + 0), 2, s2);

//   *(indices + 1) = 0;
//   s0 = (bitRead(reg, 7) ^ !(bitRead(reg, 4) ^ bitRead(reg, 1)));
//   s1 = (bitRead(reg, 6) ^ !(bitRead(reg, 3) ^ bitRead(reg, 0)));
//   s2 = (bitRead(reg, 5) ^ !(bitRead(reg, 2) ^ bitRead(reg, 7)));
//   bitWrite(*(indices + 1), 0, s0);
//   bitWrite(*(indices + 1), 1, s1);
//   bitWrite(*(indices + 1), 2, s2);

//   *(indices + 2) = 0;
//   s0 = (bitRead(reg, 3) ^ !(bitRead(reg, 6) ^ bitRead(reg, 1)));
//   s1 = (bitRead(reg, 0) ^ !(bitRead(reg, 2) ^ bitRead(reg, 5)));
//   s2 = (bitRead(reg, 5) ^ !(bitRead(reg, 7) ^ bitRead(reg, 1)));
//   bitWrite(*(indices + 2), 0, s0);
//   bitWrite(*(indices + 2), 1, s1);
//   bitWrite(*(indices + 2), 2, s2);

//   *(indices + 3) = 0;
//   s0 = (bitRead(reg, 0) ^ !(bitRead(reg, 6) ^ bitRead(reg, 5)));
//   s1 = (bitRead(reg, 2) ^ !(bitRead(reg, 1) ^ bitRead(reg, 7)));
//   s2 = (bitRead(reg, 4) ^ !(bitRead(reg, 3) ^ bitRead(reg, 1)));
//   bitWrite(*(indices + 3), 0, s0);
//   bitWrite(*(indices + 3), 1, s1);
//   bitWrite(*(indices + 3), 2, s2);
// }


uint8_t getDrunkenIndex()
{
  static int8_t drunkStep(0);
  int8_t rn(random(0, 101));

  if (rn > 90)
  {
    rn = 3;
  }
  else if (rn > 70)
  {
    rn = 2;
  }
  else
  {
    rn = 1;
  }

  if (random(0, 2))
  {
    rn *= -1;
  }

  drunkStep += rn;
  if (drunkStep < 0)
  {
    drunkStep += 8;
  }
  else if (drunkStep > 7)
  {
    drunkStep -= 8;
  }

  return drunkStep;
}


// DAC 0: Faders & register
// DAC 1: Faders & ~register
// DAC 2: abs(DAC 1 - DAC 0)
// DAC 3: DAC 0 if reg & BIT0 else no change from last value
void expandVoltages(uint8_t shiftReg)
{
  uint16_t faderVals[8];
  uint16_t noteVals[4]{0, 0, 0, 0};

  for (uint8_t ch(0); ch < NUM_FADERS; ++ch)
  {
    faderVals[ch] = (pCV + ch)->readActiveCtrl();

    if (bitRead(shiftReg, ch))
    {
      // CV A: Faders & register
      noteVals[0] += faderVals[ch];
    }
    else
    {
      // CV B: Faders & ~register
      noteVals[1] += faderVals[ch];
    }
    printf("Fader %u: %u\n", ch, faderVals[ch]);
  }

  // CV C: abs(CV A - CV B)
  if (noteVals[0] > noteVals[1])
  {
    noteVals[2] = noteVals[0] - noteVals[1];
  }
  else
  {
    noteVals[2] = noteVals[1] - noteVals[0];
  }

  static uint16_t lastCV_D(noteVals[0]);
  if (bitRead(shiftReg, 0))
  {
    lastCV_D = noteVals[0];
  }
  noteVals[3] = lastCV_D;

  for (uint8_t ch(0); ch < 4; ++ch)
  {
    uint16_t val(noteToDacVal(ch, noteVals[ch]));
    printf("DAC %u note: %u, val: %u\n", ch, noteVals[ch], val);
    writeRawValToDac(ch, val);
  }

  uint8_t jnkm(~dacRegister);
  voltsExp.outputVoltage(jnkm);
  printf("Jank : %u\n", jnkm);
}


uint8_t pulseIt(int8_t step, uint8_t inputReg)
{
  // Bit 0 from shift register bit 0
  uint8_t outputReg(inputReg & BIT0);

  // Bit 1 is !Bit 0
  outputReg |= (~outputReg << 1) & BIT1;

  // Bit 2 is high if register value is >= some random number
  if (random() % 256 <= inputReg)
  {
    outputReg |= BIT2;
  }

  // Bit 3 is !Bit2
  outputReg |= (~outputReg << 1) & BIT3;

  outputReg |= (((inputReg & BIT1) >> 1) ^ ((inputReg & BIT6) >> 6) << 4) & BIT4;
  outputReg |= (((inputReg & BIT0) >> 0) ^ ((inputReg & BIT4) >> 4) << 5) & BIT5;
  outputReg |= (((inputReg & BIT3) >> 3) ^ ((inputReg & BIT7) >> 7) << 6) & BIT6;
  outputReg |= (((inputReg & BIT2) >> 2) ^ ((inputReg & BIT5) >> 5) << 7) & BIT7;

  return outputReg;
}


void turingStep(int8_t stepAmount)
{
  // Turn off the triggers in case a new clock came in before they expired
  printf("----------------------------\n");
  if (checkResetFlag())
  {
    alan.reset();
  }

  int8_t alanStep(alan.iterate(stepAmount));
  if (set_bit_0)
  {
    set_bit_0 = false;
    uint8_t idx(stepAmount > 0 ? 0 : 7);
    alan.writeBit(idx, 1);
    println("set bit 0");
  }
  else if (clear_bit_0)
  {
    clear_bit_0 = false;
    uint8_t idx(stepAmount > 0 ? 0 : 7);
    alan.writeBit(idx, 0);
    println("clear bit 0");
  }

  // Get the Turing register pattern value
  uint8_t tmpReg(alan.getOutput());

  // Update voltages so they are ready when the triggers update
  if (!shift)
  {
    setDacRegister(tmpReg);
  }
  else
  {
    uint8_t len(alan.getLength());
    if (len <= 8)
    {
      setDacRegister(0x01 << len);
    }
    else
    {
      setDacRegister(~(0x01 << (len - 8)));
    }
  }

  expandVoltages(tmpReg);
  setFaderRegister(tmpReg & faderLockStateReg);

  uint8_t trigReg(pulseIt(alanStep, tmpReg));
  setTrigRegister(trigReg);

  leds.clock();
  triggers.clock();

  print("register:  ");
  printBits(tmpReg);
  print("output  :  ");
  printBits(trigReg);
}


void handleEncoder()
{
  // Check for new events from our encoder and clear the "ready" flag if there are
  switch(encoderInterface.getEvent())
  {
    case encEvnts :: DblClick:
      shift = !shift;
      // printf("LENGTH: %u\n", alan.getLength());
      break;

    case encEvnts :: Click:
      alan.reset();
      break;

    case encEvnts :: Hold:
      break;

    case encEvnts :: Right:
      if (shift)
      {
        alan.lengthPLUS();
      }
      break;

    case encEvnts :: Left:
      if (shift)
      {
        alan.lengthMINUS();
      }
      break;

    case encEvnts :: Press:
      break;

    case encEvnts :: ClickHold:
      break;

    case encEvnts :: ShiftLeft:
      if (CASSIDEBUG) printf("cv a: %u\n", cvA.readRaw());
      break;

    case encEvnts :: ShiftRight:
      if (CASSIDEBUG) printf("cv b: %u\n", cvB.readRaw());
      break;

    case encEvnts :: NUM_ENC_EVNTS:
      break;

    default:
      break;
  }
}


void loop()
{
  ButtonState clickies[]{ writeLow.readAndFree(), writeHigh.readAndFree() };
  if (checkClockFlag() == 1)
  {
    if (clickies[0] == ButtonState :: Held)
    {
      clear_bit_0 = true;
    }
    else if(clickies[1] == ButtonState :: Held)
    {
      set_bit_0 = true;
    }
    turingStep((cvB.readRaw() > 2047) ? -1 : 1);
  }
  else if (checkClockFlag() == -1)
  {
    triggers.allOff();
  }


  // Double-click to change fader octave range (1 to 3 octaves)
  if ((OctaveRange > 1) && (clickies[0] == ButtonState :: DoubleClicked))
  {
    --OctaveRange;
    faderLocksChanged = true;
  }
  else if ((OctaveRange < 3) && (clickies[1] == ButtonState :: DoubleClicked))
  {
    ++OctaveRange;
    faderLocksChanged = true;
  }

  if (faderLocksChanged)
  {
    for (uint8_t fd = 0; fd < NUM_FADERS; ++fd)
    {
      (pCV + fd)->select(OctaveRange - 1);
      if (!faderInitValsSet[OctaveRange - 1])
      {
        (pCV + fd)->overWrite();
        faderInitValsSet[OctaveRange - 1] = true;
      }
      bitWrite(faderLockStateReg,
               fd,
               (pCV + fd)->pACTIVE->getLockState() == LockState :: STATE_UNLOCKED);
    }

    faderLocksChanged = false;
  }

  // Single click to set/clear BIT0
  if (clickies[0] == ButtonState :: Clicked)
  {
      clear_bit_0 = true;
  }
  else if (clickies[1] == ButtonState :: Clicked)
  {
    set_bit_0 = true;
  }

  for (uint8_t fd = 0; fd < NUM_FADERS; ++fd)
  {
    (pCV + fd)->readActiveCtrl();
    bitWrite(faderLockStateReg,
             fd,
             (pCV + fd)->pACTIVE->getLockState() == LockState :: STATE_UNLOCKED);
    leds.tempWrite(dacRegister & faderLockStateReg);
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
  printf("ch: %u\n", selch);
  while (true)
  {
    if (writeHigh.readAndFree())
    {
      writeRawValToDac(selch, 0);
      ++selch;
      if (selch == 4) selch = 0;
      selreg = 0 | (1 << selch);
      (pCV + selch)-> select(0);
      leds.tempWrite(selreg, 0);
      leds.tempWrite(selreg, 1);
      printf("ch: %u\n", selch);
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
    MCP4728.setChannelValue(DAC_CH[selch],
                            outval,
                            MCP4728_VREF_INTERNAL,
                            MCP4728_GAIN_2X);
    if (writeLow.readAndFree())
    {
      printf("%u\n", outval);
    }
  }
}