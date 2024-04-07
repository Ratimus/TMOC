// ------------------------------------------------------------------------
//                       Turing Machine On Crack
//
//   A software re-imagining of Tom Whitwell / Music Thing Modular's Turing
//   Machine random looping sequencer, plus Voltages and Pulses expanders.
//
//   Based on an ESP32 microcontroller and featuring four 12-bit DACs, one
//   8-bit DAC with analog scale and offset and an inverting output, and
//   eight gate/trigger outputs
//
// main.cpp
//
// Nov. 2022
// Ryan "Ratimus" Richardson
// ------------------------------------------------------------------------

// Set TRUE to enable Serial printing (and whatever else)
#include <Arduino.h>
#include <Wire.h>
#include "TMOC_HW.h"
#include "DacESP32.h"
#include "RatFuncs.h"
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

// Bit 0 set/clear
MagicButton writeHigh(TOGGLE_UP, 1, 1);
MagicButton writeLow(TOGGLE_DOWN, 1, 1);

ClickEncoder encoder(ENC_A,
                     ENC_B,
                     ENC_SW,
                     ENC_STEPS_PER_NOTCH,
                     ENC_ACTIVE_LOW);

ClickEncoderInterface encoderInterface(encoder,
                                       ENC_SENSITIVITY);

ESP32AnalogRead cvA;        // "CV" input
ESP32AnalogRead cvB;        // "NOISE" input
ESP32AnalogRead turing;     // "LOOP" variable resistor

uint8_t OctaveRange(1);
uint8_t currentBank(0);
uint8_t loadSlot(0);
uint8_t saveSlot(0);

enum mode_type {
  PERFORMANCE_MODE,
  SHOW_LENGTH_MODE,
  PATTERN_SAVE_MODE,
  PATTERN_LOAD_MODE,
  NUM_MODES
};

mode_type currentMode(mode_type :: PERFORMANCE_MODE);

bool regLedsDirty(0);

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
void savePattern();
void loadPattern();

volatile uint16_t millisTimer(0);
volatile uint8_t flashTimer(0);

uint8_t getFlashTimer()
{
  uint8_t timerVal;
  cli();
  timerVal = flashTimer;
  sei();
  uint8_t retval(0);
  if ((timerVal / 4) % 2)
  {
    bitWrite(retval, 0, 1);
  }

  if ((timerVal / 25) % 2)
  {
    bitWrite(retval, 1, 1);
  }
  return retval;
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
    if (val != gateInVals[gate])
    {
      newGateFlags[gate] = (int8_t)val - (int8_t)gateInVals[gate];
    }
    gateInVals[gate] = val;
  }

  ++millisTimer;
  if (millisTimer == 1000)
  {
    millisTimer = 0;
  }
  flashTimer = millisTimer / 10;
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

void setup()
{
#ifdef CASSIDEBUG
  Serial.begin(115200);
#endif

  delay(100);

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
  cvA.attach(CV_IN_A);
  cvB.attach(CV_IN_B);
  turing.attach(LOOP_CTRL);

  // Gate inputs
  pinMode(CLOCK_IN,         INPUT);
  pinMode(RESET_IN,         INPUT);

  // Clear trigger output register
  setTrigRegister(0);
  triggers.clock();

  // Set up external 8 channel ADC
  initADC();
  ONE_OVER_ADC_MAX = 1.0f / pADC0->maxValue();

  // Set up external 4 channel DAC
  dbprintln("MCP4728 test...");
  if (!MCP4728.begin(0x64))
  {
    dbprintln("Failed to find MCP4728 chip");
    while (1) { delay(1); }
  }
  dbprintln("MCP4728 chip initialized");

  for (uint8_t ch(0); ch < 4; ++ch)
  {
    MCP4728.setChannelValue((MCP4728_channel_t)ch, 0, MCP4728_VREF_INTERNAL);
  }

  // Set up master clock
  timer1 = timerBegin(1, 80, true);
  timerAttachInterrupt(timer1, &onTimer1, true);
  timerAlarmWrite(timer1, ONE_KHZ_MICROS, true);
  timerAlarmEnable(timer1);

  // Initialize virtual fader bank sample buffers
  initFaders(fadersRunning);
  delay(100);

  // Pre-fill all pattern registers
  uint16_t randomReg((random(1, 255) << 8) | random(1, 255));
  for (uint8_t bk(0); bk < NUM_BANKS; ++bk)
  {
    alan.writeToRegister(~bk, bk);
    // alan.writeToRegister(randomReg, bk);
    for (uint8_t fd = 0; fd < NUM_FADERS; ++fd)
    {
      (pCV + fd)->select(bk);
      (pCV + fd)->overWrite();
    }
  }

  for (uint8_t fd = 0; fd < NUM_FADERS; ++fd)
  {
      // Don't light up locked faders
    (pCV + fd)->select(0);
    (pCV + fd)->pACTIVE->sample();
    bitWrite(faderLockStateReg, fd, (pCV + fd)->pACTIVE->getLockState() == STATE_UNLOCKED);
    dbprintf("CH %u: %u\n", fd, (pCV + fd)->readActiveCtrl());
  }
  // Run manual calibration routine
  // calibrate();


  // Set pattern LEDs to display current pattern
  setDacRegister(alan.getOutput());
  setFaderRegister(dacRegister & faderLockStateReg);
  leds.clock();
}


// Does what it says on the tin; writes [val] to channel [ch] of external DAC
void writeRawValToDac(uint8_t ch, uint16_t val)
{
  MCP4728.setChannelValue(DAC_CH[ch], val, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
}


// Translates note values to raw DAC outputs using calibration tables
uint16_t noteToDacVal(uint8_t ch, uint16_t note)
{
  // Get the calibration table for this channel
  const uint16_t *calTable(DAC_CAL_TABLES[DAC_CH[ch]]);

  // Get the octave from the absolute note number
  uint8_t octave(note / 12);
  if (octave > CAL_TABLE_HIGH_OCTAVE)
  {
    octave = CAL_TABLE_HIGH_OCTAVE;
  }

  // Use the calibration table to determine how much you'd need to add to the DAC value
  // in order to go up an octave from the current octave. Divide that by 12 to get the
  // value of a semitone within the current octave.
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

  // Determine which semitone we want within the current octave, then multiply that
  // by the DAC value of a semitone. Add it to the DAC value for the octave and we've
  // got our output value.
  uint16_t semiTone(note - (octave * 12));
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


// Updates main horizontal LED array to display current pattern (in
// performance mode) or status (in one of the editing modes)
void updateRegLeds()
{
  regLedsDirty = 0;
  if (currentMode == mode_type :: PERFORMANCE_MODE)
  {
    leds.setReg(dacRegister, 1);
    leds.clock();
    return;
  }

  if (currentMode == mode_type :: SHOW_LENGTH_MODE)
  {
    uint8_t len(alan.getLength());
    if (len <= 8)
    {
      leds.setReg(0x01 << (len - 1), 1);
    }
    else
    {
      leds.setReg(~(0x01 << (len - 9)), 1);
    }
    leds.clock();
    return;
  }

  uint8_t flashTimer(getFlashTimer());

  if (currentMode == mode_type :: PATTERN_LOAD_MODE)
  {
    if (flashTimer & BIT0)
    {
      leds.setReg(0, 1);
    }
    else
    {
      leds.setReg(0x01 << loadSlot, 1);
    }
    leds.clock();
    return;
  }

  if (currentMode == mode_type :: PATTERN_SAVE_MODE)
  {
    if (flashTimer & BIT1)
    {
      leds.setReg(0, 1);
    }
    else
    {
      leds.setReg(0x01 << saveSlot, 1);
    }
    leds.clock();
    return;
  }
}


// "Drunken Walk" algorithm - randomized melodies but notes tend to cluster together
// TODO: Once this is implemented as a sequencer mode, add control over the degree of
// deviation
uint8_t getDrunkenIndex()
{
  static int8_t drunkStep(0);

  // How big of a step are we taking?
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

  // Are we stepping forward or backward?
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


// Melodic algorithm inspired by Mystic Circuits' "Leaves" sequencer
void iThinkYouShouldLeaf(uint8_t shiftReg)
{
  uint16_t faderVals[8];
  uint16_t noteVals[4]{0, 0, 0, 0};

  uint8_t faderMasks[4]{
    uint8_t(0b00001111) & shiftReg,
    uint8_t(0b00111100) & shiftReg,
    uint8_t(0b11110000) & shiftReg,
    uint8_t(0b11000011) & shiftReg
  };

  // This is the note we're going to write to the analog scale, invert,
  // and offset circuit
  uint16_t internalDacNote(0);

  // Figure out what we're going to set our 4 external DACs to
  for (uint8_t ch(0); ch < NUM_FADERS; ++ch)
  {
    faderVals[ch] = (pCV + ch)->readActiveCtrl();
    uint8_t channelMask(0x01 << ch);
    for (auto fd(0); fd < 4; ++fd)
    {
      if (faderMasks[fd] & channelMask)
      {
        noteVals[fd] += faderVals[ch];
      }
    }
    if (shiftReg & channelMask)
    {
      internalDacNote += faderVals[ch];
    }
  }

  for (auto ch(0); ch < 4; ++ch)
  {
    uint16_t val(noteToDacVal(ch, noteVals[ch]));
    // dbprintf("DAC %u note: %u, val: %u\n", ch, noteVals[ch], val);
    writeRawValToDac(ch, val);
  }

  // Translate the note to a 12 bit DAC value, then convert that to an 8 bit value
  // since the internal DAC is only 8 bits
  uint16_t writeVal_12(noteToDacVal(0, internalDacNote));
  uint8_t writeVal_8(map(writeVal_12, 0, 4096, 0, 256));
  voltsExp.outputVoltage(writeVal_8);
  // dbprintf("writeVal_8 : %u\n", writeVal_8);
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
    // dbprintf("Fader %u: %u\n", ch, faderVals[ch]);
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

  // CV D: same as CV A, but only changes when BIT 0 is high
  static uint16_t lastCV_D(noteVals[0]);
  if (bitRead(shiftReg, 0))
  {
    lastCV_D = noteVals[0];
  }
  noteVals[3] = lastCV_D;

  // Write the output values to the external DACs
  for (uint8_t ch(0); ch < 4; ++ch)
  {
    uint16_t val(noteToDacVal(ch, noteVals[ch]));
    dbprintf("DAC %u note: %u, val: %u\n", ch, noteVals[ch], val);
    writeRawValToDac(ch, val);
  }

  // Write the inverse of the pattern register to the internal (8 bit) DAC
  uint8_t writeVal_8(~shiftReg);
  voltsExp.outputVoltage(writeVal_8);
  // dbprintf("writeVal_8 : %u\n", writeVal_8);
}


// Make the trigger outputs do something interesting. Ideally, they'll all be related to
// the main pattern register, but still different enough to justify having 8 of them.
// I messed around with a bunch of different bitwise transformations and eventually came
// across a few that I think sound cool.
uint8_t pulseIt(int8_t step, uint8_t inputReg)
{
  // Bit 0 from shift register; Bit 1 is !Bit 0
  uint8_t outputReg(inputReg & BIT0);
  outputReg |= (~outputReg << 1) & BIT1;

  outputReg |= ((((inputReg & BIT0) >> 0) & ((inputReg & BIT3) >> 3)) << 2) & BIT2;
  outputReg |= ((((inputReg & BIT2) >> 2) & ((inputReg & BIT7) >> 7)) << 3) & BIT3;

  outputReg |= (((inputReg & BIT1) >> 1) ^ ((inputReg & BIT6) >> 6) << 4) & BIT4;
  outputReg |= (((inputReg & BIT0) >> 0) ^ ((inputReg & BIT4) >> 4) << 5) & BIT5;
  outputReg |= (((inputReg & BIT3) >> 3) ^ ((inputReg & BIT7) >> 7) << 6) & BIT6;
  outputReg |= (((inputReg & BIT2) >> 2) ^ ((inputReg & BIT5) >> 5) << 7) & BIT7;

  return outputReg;
}


// Got an external clock pulse? Do the thing
void turingStep(int8_t stepAmount)
{
  // Handle a pending reset
  dbprintf("----------------------------\n");
  if (checkResetFlag())
  {
    alan.reset();
  }

  // Iterate the sequencer to the next state
  int8_t alanStep(alan.iterate(stepAmount));

  // Handle the write/clear toggle switch
  if (set_bit_0)
  {
    set_bit_0 = false;
    uint8_t idx(stepAmount > 0 ? 0 : 7);
    alan.writeBit(idx, 1);
    dbprintln("set bit 0");
  }
  else if (clear_bit_0)
  {
    clear_bit_0 = false;
    uint8_t idx(stepAmount > 0 ? 0 : 7);
    alan.writeBit(idx, 0);
    dbprintln("clear bit 0");
  }

  // Get the Turing register pattern value
  dacRegister = alan.getOutput();
  regLedsDirty = 1;

  // Update all the various outputs
  expandVoltages(dacRegister);
  setFaderRegister(dacRegister & faderLockStateReg);
  setTrigRegister(pulseIt(alanStep, dacRegister));


  triggers.clock();

  dbprint("register:  ");
  printBits(dacRegister);
  dbprint("output  :  ");
  printBits(trgRegister);
}


/*
standard + click -> show length
showing length + encoder -> change length
showing length + click -> back to standard

standard + double click -> show pattern selection
show selection + encoder -> select next pattern
show selection + click -> change to next pattern and back to standard

standard + shift + encoder -> need to figure out shift/hold timing and state transitions.
*/

void handleEncoder()
{
  regLedsDirty = 1;

  // Check for new events from our encoder and clear the "ready" flag if there are
  switch(encoderInterface.getEvent())
  {
    case encEvnts :: Click:
      if (currentMode == mode_type :: PERFORMANCE_MODE)
      {
        dbprintln("perform -> show length");
        currentMode = SHOW_LENGTH_MODE;
      }
      else if (currentMode == mode_type :: SHOW_LENGTH_MODE)
      {
        dbprintf("length = %u -> perform\n", alan.getLength());
        currentMode = PERFORMANCE_MODE;
      }
      else
      {
        regLedsDirty = 0;
      }
      return;

    case encEvnts :: DblClick:
      if (currentMode == mode_type :: PERFORMANCE_MODE)
      {
        currentMode = PATTERN_LOAD_MODE;
      }
      else if (currentMode == mode_type :: PATTERN_LOAD_MODE)
      {
        loadPattern();
        currentMode = PERFORMANCE_MODE;
      }
      else
      {
        regLedsDirty = 0;
      }
      return;

    case encEvnts :: Hold:
      if (currentMode == mode_type :: PERFORMANCE_MODE)
      {
        currentMode = PATTERN_SAVE_MODE;
      }
      else if (currentMode == mode_type :: PATTERN_SAVE_MODE)
      {
        savePattern();
        currentMode = PERFORMANCE_MODE;
      }
      else
      {
        regLedsDirty = 0;
      }
      return;

    case encEvnts :: Right:
      switch (currentMode)
      {
        case mode_type :: PERFORMANCE_MODE:
          turingStep(1);
          break;

        case mode_type :: SHOW_LENGTH_MODE:
          alan.lengthPLUS();
          break;

        case mode_type :: PATTERN_LOAD_MODE:
          ++loadSlot;
          loadSlot %= NUM_BANKS;
          dbprintf("pattern to load: %u\n", loadSlot);
          break;

        case mode_type :: PATTERN_SAVE_MODE:
          ++saveSlot;
          saveSlot %= NUM_BANKS;
          dbprintf("save to slot: %u\n", saveSlot);
          break;

        default:
          regLedsDirty = 0;
          break;
      }
      return;

    case encEvnts :: Left:
      switch (currentMode)
      {
        case mode_type :: PERFORMANCE_MODE:
          turingStep(-1);
          break;

        case mode_type :: SHOW_LENGTH_MODE:
          alan.lengthMINUS();
          break;

        case mode_type :: PATTERN_LOAD_MODE:
          --loadSlot;
          loadSlot %= NUM_BANKS;
          dbprintf("pattern to load: %u\n", loadSlot);
          break;

        case mode_type :: PATTERN_SAVE_MODE:
          --saveSlot;
          saveSlot %= NUM_BANKS;
          dbprintf("save to slot: %u\n", saveSlot);
          break;

        default:
          regLedsDirty = 0;
          break;
      }
      return;

    case encEvnts :: ClickHold:
      dbprintln("click + hold");
      break;

    case encEvnts :: ShiftLeft:
      dbprintln("shift left");
      break;

    case encEvnts :: ShiftRight:
      dbprintln("shift right");
      break;

    case encEvnts :: Press:
    case encEvnts :: NUM_ENC_EVNTS:
    default:
      break;
  }
}


// TODO: I updated my MagicButton class a while ago to only register a 'Held' state after you
// release, so I need to bring that back as an option if I want it to function that way
void loop()
{
  ButtonState clickies[]{ writeLow.readAndFree(), writeHigh.readAndFree() };
  if (checkClockFlag() == 1)
  {
    // Single clicks on the toggle will set/clear the next bit, but we also want to
    // be able to hold it down and write or clear the entire register
    turingStep((cvB.readRaw() > 2047) ? -1 : 1);
  }
  else if (checkClockFlag() == -1)
  {
    triggers.allOff();
  }

  if (regLedsDirty)
  {
    updateRegLeds();
  }

  // Double-click to change fader octave range (1 to 3 octaves)
  if ((OctaveRange > 1) && (clickies[0] == ButtonState :: DoubleClicked))
  {
    --OctaveRange;
    setRange(OctaveRange);
  }
  else if ((OctaveRange < 3) && (clickies[1] == ButtonState :: DoubleClicked))
  {
    ++OctaveRange;
    setRange(OctaveRange);
  }

  // Single click to set/clear BIT0
  if (clickies[0] == ButtonState :: Clicked || clickies[0] == ButtonState :: Held)
  {
      clear_bit_0 = true;
  }
  else if (clickies[1] == ButtonState :: Clicked || clickies[1] == ButtonState :: Held)
  {
    if (currentMode == mode_type :: PERFORMANCE_MODE)
    {
      set_bit_0 = true;
    }
    else
    {
      currentMode = mode_type :: PERFORMANCE_MODE;
      regLedsDirty = true;
    }
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

void savePattern()
{
  for (uint8_t fd = 0; fd < NUM_FADERS; ++fd)
  {
    (pCV + fd)->copySettings(saveSlot);
  }
  alan.savePattern(saveSlot);
  dacRegister = alan.getOutput();
  regLedsDirty = 1;
}


void loadPattern()
{
  uint8_t active(0);
  for (uint8_t fd = 0; fd < NUM_FADERS; ++fd)
  {
    (pCV + fd)->select(loadSlot);
  }
  alan.loadPattern(loadSlot);
  dacRegister = alan.getOutput();
  currentBank = loadSlot;
  regLedsDirty = 1;
}

void calibrate()
{
  uint8_t selch(0);
  uint8_t selreg(1);
  leds.tempWrite(1, 0);
  leds.tempWrite(1, 1);

  uint16_t outval(0);
  (pCV + selch)->select(0);
  dbprintf("ch: %u\n", selch);
  while (true)
  {
    if (writeHigh.readAndFree())
    {
      writeRawValToDac(selch, 0);
      ++selch;

      // Here, I sacrifice readability for the sake of making a cool-looking block
      if (selch == 4) selch = 0;
      selreg = 0 | (1 << selch);
      (pCV + selch)-> select(0);
      leds.tempWrite(selreg, 0);
      leds.tempWrite(selreg, 1);
      dbprintf("ch: %u\n", selch);
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
      dbprintf("%u\n", outval);
    }
  }
}