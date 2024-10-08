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

// define RATDEBUG to enable Serial printing (and whatever else) - see RatFuncs.h
// #ifndef RATDEBUG
// #define RATDEBUG
// #endif

#include <Arduino.h>
#include <Wire.h>
#include "TMOC_HW.h"
#include <GateIn.h>
#include <DacESP32.h>
#include <RatFuncs.h>
#include "OutputDac.h"
#include <CtrlPanel.h>
#include <bitHelpers.h>
#include <MagicButton.h>
#include <ClickEncoder.h>
#include "TuringRegister.h"
#include <OutputRegister.h>
#include <ESP32AnalogRead.h>
#include <ClickEncoderInterface.h>
#include "ESP32_New_TimerInterrupt.h"


// Bit 0 set/clear
MagicButton writeHigh(TOGGLE_UP, 1, 1);
MagicButton writeLow(TOGGLE_DOWN, 1, 1);

// This is the device driver for our rotary encoder
ClickEncoder encoder(ENC_A,
                     ENC_B,
                     ENC_SW,
                     ENC_STEPS_PER_NOTCH,
                     ENC_ACTIVE_LOW);

// And this is the interface to the encoder's device driver
ClickEncoderInterface encoderInterface(encoder,
                                       ENC_SENSITIVITY);

// Different editing/playback modes for our rotary encoder
enum class mode_type {
  PERFORMANCE_MODE,
  CHANGE_LENGTH_MODE,
  PATTERN_SAVE_MODE,
  PATTERN_LOAD_MODE,
  NUM_MODES
};

mode_type currentMode(mode_type :: PERFORMANCE_MODE);

// Use the onboard ADCs for external control voltage & the main control
ESP32AnalogRead cvA;        // "CV" input
ESP32AnalogRead cvB;        // "NOISE" input
ESP32AnalogRead turing;     // "LOOP" variable resistor

// Sequencer state variables
uint8_t loadSlot(0);
uint8_t saveSlot(0);
uint8_t OctaveRange(1);
uint8_t currentBank(0);

bool set_bit_0(0);
bool clear_bit_0(0);
bool regLedsDirty(0);

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
GateInArduino gates(NUM_GATES_IN, GATE_PIN, true);

hw_timer_t *timer1(nullptr);   // Timer library takes care of telling this where to point

float ONE_OVER_ADC_MAX(0.0);

// Note: you're still gonna need to clock this before it updates
void setDacRegister(uint8_t val)  { dacRegister = val; leds.setReg(dacRegister, 1);  }
void setTrigRegister(uint8_t val) { trgRegister = val; triggers.setReg(trgRegister); }
void setFaderRegister(uint8_t val){ fdrRegister = val; leds.setReg(fdrRegister, 0);  }

void calibrate();
void savePattern();
void loadPattern();

// Volatile, 'cause we mess with these in ISRs
volatile uint8_t flashTimer(0);
volatile uint16_t millisTimer(0);

const uint8_t NUM_DAC_CHANNELS(4);
MultiChannelDac output(NUM_DAC_CHANNELS);

// This blinks LEDs on and off with different timings to indicate which mode you're in
// SOLID: set length (get here with a single click)
// FAST: select pattern to load (get here with a double-click)
// SLOW: select slot for saving current pattern (get here with a long press)
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
  for (uint8_t fd = 0; fd < 8; ++fd)
  {
    faderBank[fd]->service();
  }

  encoder.service();
  writeLow.service();
  writeHigh.service();

  gates.service();

  ++millisTimer;
  if (millisTimer == 1000)
  {
    millisTimer = 0;
  }
  flashTimer = millisTimer / 10;
}


void setup()
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
  cvA.attach(CV_IN_A);
  cvB.attach(CV_IN_B);
  turing.attach(LOOP_CTRL);

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

  // Set up master clock
  timer1 = timerBegin(1, 80, true);
  timerAttachInterrupt(timer1, &onTimer1, true);
  timerAlarmWrite(timer1, ONE_KHZ_MICROS, true);
  timerAlarmEnable(timer1);

  // Set pattern LEDs to display current pattern
  setDacRegister(alan.getOutput());
  setFaderRegister(dacRegister & faderLockStateReg);
  leds.clock();
}


// Updates main horizontal LED array to display current pattern (in
// performance mode) or status (in one of the editing modes)
void updateRegLeds()
{
  // Check whether each individual fader is unlocked; don't light them up unless they are
  for (auto fd(0); fd < NUM_FADERS; ++fd)
  {
    bitWrite(faderLockStateReg, fd, faderBank[fd]->getLockState() == LockState :: STATE_UNLOCKED);
  }
  leds.setReg(dacRegister & faderLockStateReg, 0);

  regLedsDirty = 0;
  if (currentMode == mode_type :: PERFORMANCE_MODE)
  {
    // Display the current register/pattern value
    leds.setReg(dacRegister, 1);
    leds.clock();
    return;
  }

  if (currentMode == mode_type :: CHANGE_LENGTH_MODE)
  {
    // Light up the LED corresponding to pattern length. For length > 8,
    // light up all LEDs and dim the active one
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

  // Fast blink = LOAD, slower blink = SAVE
  uint8_t flashTimer(getFlashTimer());

  if (currentMode == mode_type :: PATTERN_LOAD_MODE)
  {
    // Flash LED corresponding to selected bank
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
    // Flash LED corresponding to selected slot
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


// TODO: decide what controls I want to use to activate this alternate mode
// Melodic algorithm inspired by Mystic Circuits' "Leaves" sequencer
void iThinkYouShouldLeaf(uint8_t shiftReg)
{
  uint16_t faderVals[8];
  uint16_t noteVals[4]{0, 0, 0, 0};

  uint8_t faderMasks[4];
  faderMasks[0] = shiftReg & 0b00001111;
  faderMasks[1] = shiftReg & 0b00111100;
  faderMasks[2] = shiftReg & 0b11110000;
  faderMasks[3] = shiftReg & 0b11000011;

  // This is the note we're going to write to the analog scale, invert,
  // and offset circuit
  uint16_t internalDacNote(0);

  // Figure out what we're going to set our 4 external DACs to
  for (uint8_t ch(0); ch < NUM_FADERS; ++ch)
  {
    faderVals[ch] = faderBank[ch]->read();
    dbprintf("Fader %u: %u\n", ch, faderVals[ch]);
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
    output.setChannelNote(ch, noteVals[ch]);
  }

  // Translate the note to a 12 bit DAC value, then convert that to an 8 bit value
  // since the internal DAC is only 8 bits
  // uint16_t writeVal_12(noteToDacVal(0, internalDacNote));
  // uint8_t writeVal_8(map(writeVal_12, 0, 4096, 0, 256));
  // voltsExp.outputVoltage(writeVal_8);
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
    faderVals[ch] = faderBank[ch]->read();
    dbprintf("Fader %u: %u\n", ch, faderVals[ch]);

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
    dbprintf("Channel %u: note %u\n", ch, noteVals[ch]);

    output.setChannelNote(ch, noteVals[ch]);
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

#define RESET_FLAG 1
#define CLOCK_FLAG 0

// Got an external clock pulse? Do the thing
void turingStep(int8_t stepAmount)
{
  // Handle a pending reset
  // dbprintf("----------------------------\n");
  if (gates.readRiseFlag(RESET_FLAG))
  {
    dbprintf("RESET!!!\n");
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
  }
  else if (clear_bit_0)
  {
    clear_bit_0 = false;
    uint8_t idx(stepAmount > 0 ? 0 : 7);
    alan.writeBit(idx, 0);
  }

  // Get the Turing register pattern value
  dacRegister = alan.getOutput();
  regLedsDirty = 1;

  // Update all the various outputs
  expandVoltages(dacRegister);
  setFaderRegister(dacRegister & faderLockStateReg);
  setTrigRegister(pulseIt(alanStep, dacRegister));
  triggers.clock();
}


/*
for save/load modes, hold the write/clear toggle switch up to cancel and return to performance
mode without saving/loading a new pattern

performance mode + click -> edit length
edit length + encoder -> change length
edit length + click -> return to performance

performance mode + double click -> show pattern selection
show selection + encoder -> selectActiveBank next pattern
show selection + double click -> change to next pattern and return to performance

performance mode + hold -> show save slot
show save slot + encoder -> select save slot
show save slot + hold -> save current pattern/settings to selected slot and return to performance

performance mode + encoder -> clock the sequencer fwd/rev

standard + shift + encoder -> rotate pattern register without advancing step counter
*/

void handleEncoder()
{
  // void loop() will update LEDs if we set this flag. Clear it if we end up not changing modes
  regLedsDirty = 1;

  // Check for new events from our encoder and clear the "ready" flag if there are
  switch(encoderInterface.getEvent())
  {
    case encEvnts :: Click:
      if (currentMode == mode_type :: PERFORMANCE_MODE)
      {
        dbprintln("perform -> show length");
        currentMode = mode_type :: CHANGE_LENGTH_MODE;
      }
      else if (currentMode == mode_type :: CHANGE_LENGTH_MODE)
      {
        dbprintf("length = %u -> perform\n", alan.getLength());
        currentMode = mode_type :: PERFORMANCE_MODE;
      }
      else
      {
        regLedsDirty = 0;
      }
      return;

    case encEvnts :: DblClick:
      if (currentMode == mode_type :: PERFORMANCE_MODE)
      {
        currentMode = mode_type :: PATTERN_LOAD_MODE;
      }
      else if (currentMode == mode_type :: PATTERN_LOAD_MODE)
      {
        loadPattern();
        currentMode = mode_type :: PERFORMANCE_MODE;
      }
      else
      {
        regLedsDirty = 0;
      }
      return;

    case encEvnts :: Hold:
      if (currentMode == mode_type :: PERFORMANCE_MODE)
      {
        currentMode = mode_type :: PATTERN_SAVE_MODE;
      }
      else if (currentMode == mode_type :: PATTERN_SAVE_MODE)
      {
        savePattern();
        currentMode = mode_type :: PERFORMANCE_MODE;
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
          dbprintf("step %d of %d\n", alan.getStep(), alan.getLength());
          break;

        case mode_type :: CHANGE_LENGTH_MODE:
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
          dbprintf("step %d of %d\n", alan.getStep(), alan.getLength());
          break;

        case mode_type :: CHANGE_LENGTH_MODE:
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
      alan.reAnchor();
      dbprintf("step %d of %d\n", alan.getStep(), alan.getLength());
      break;

    case encEvnts :: ShiftLeft:
      break;

    case encEvnts :: ShiftRight:
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
  if (gates.readRiseFlag(CLOCK_FLAG))
  {
    dbprintf("CLOCK!\n");
    // Single clicks on the toggle will set/clear the next bit, but we also want to
    // be able to hold it down and write or clear the entire register
    turingStep((cvB.readRaw() > 2047) ? -1 : 1);
  }
  else if (gates.readFallFlag(CLOCK_FLAG))
  {
    triggers.allOff();
  }

  if (regLedsDirty)
  {
    updateRegLeds();
  }

  // Double-click to change fader octave range (1 to 3 octaves)
  ButtonState clickies[]{ writeLow.readAndFree(), writeHigh.readAndFree() };
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
    faderBank[fd]->read();
    bitWrite(faderLockStateReg,
             fd,
             faderBank[fd]->getLockState() == LockState :: STATE_UNLOCKED);
    leds.tempWrite(dacRegister & faderLockStateReg);
  }

  handleEncoder();
}


// Saves the pattern register, pattern length, and current fader locations to the selected slot
void savePattern()
{
  dbprintf("copying into %u\n", saveSlot);
  for (uint8_t fd = 0; fd < NUM_FADERS; ++fd)
  {
    faderBank[fd]->saveActiveCtrl(saveSlot);
  }
  alan.savePattern(saveSlot);
  dacRegister = alan.getOutput();
  regLedsDirty = 1;
}


// Loads the pattern register, pattern length, and saved fader locations from the selected bank
void loadPattern()
{
  dbprintf("loading bank %u\n", loadSlot);
  for (uint8_t fd = 0; fd < NUM_FADERS; ++fd)
  {
    faderBank[fd]->selectActiveBank(loadSlot);
  }
  alan.loadPattern(loadSlot);
  dacRegister = alan.getOutput();
  currentBank = loadSlot;
  regLedsDirty = 1;
}

