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

#include <Arduino.h>
#include <RatFuncs.h>
#include <Wire.h>
#include "hw_constants.h"
#include "OutputDac.h"
#include <bitHelpers.h>
#include "TuringRegister.h"
#include <OutputRegister.h>
#include <ESP32AnalogRead.h>
#include "ESP32_New_TimerInterrupt.h"
#include "hwio.h"
#include "setup.h"
#include "leds.h"

#define RESET_FLAG 1
#define CLOCK_FLAG 0


void handleMode(ModeCommand modeCmd);
void savePattern(uint8_t saveSlot);
void loadPattern(uint8_t loadSlot);

void setup()
{
  setThingsUp();
}

// Got an external clock pulse? Do the thing
void onClock(int8_t stepAmount)
{
  // Handle a pending reset
  if (gates.readRiseFlag(RESET_FLAG))
  {
    dbprintf("RESET!!!\n");
    alan.reset();
  }

  // Iterate the sequencer to the next state
  int8_t alanStep(alan.iterate(stepAmount));
  if (alanStep == 0 && patternPending >= 0)
  {
    loadPattern(patternPending);
  }

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

  // Update all the various outputs
  expandVoltages(dacRegister);
  setFaderRegister(dacRegister & faderLockStateReg);
  setTrigRegister(alan.pulseIt());

  triggers.clock();
  leds.clock();
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
    onClock((cvB.readRaw() > 2047) ? -1 : 1);
  }
  else if (gates.readFallFlag(CLOCK_FLAG))
  {
    triggers.allOff();
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
    if (mode.currentMode() == mode_type :: PERFORMANCE_MODE)
    {
      set_bit_0 = true;
    }
    else
    {
      mode.cancel();
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

  ModeCommand modeCmd(mode.update());
  handleMode(modeCmd);
  updateRegLeds();
}


void handleMode(ModeCommand modeCmd)
{
  if(modeCmd.cmd == command_enum::NO_CMD)
  {
    return;
  }

  switch(modeCmd.cmd)
  {
    case command_enum::CHANGEMODE:
      break;

    case command_enum::STEP:
      if (modeCmd.val < 0)
      {
        onClock(-1);
      }
      else
      {
        onClock(1);
      }
      break;

    case command_enum::LOAD:
      patternPending = modeCmd.val;
      break;

    case command_enum::SAVE:
      savePattern(modeCmd.val);
      break;

    case command_enum::LENGTH:
      if (modeCmd.val < 0)
      {
        alan.lengthMINUS();
      }
      else
      {
        alan.lengthPLUS();
      }
      break;

    case command_enum::LEDS:
      break;

    case command_enum::NO_CMD:
    default:
      break;
  }
}

// Saves the pattern register, pattern length, and current fader locations to the selected slot
void savePattern(uint8_t saveSlot)
{
  dbprintf("copying into %u\n", saveSlot);
  for (uint8_t fd = 0; fd < NUM_FADERS; ++fd)
  {
    faderBank[fd]->saveActiveCtrl(saveSlot);
  }
  alan.savePattern(saveSlot);
  dacRegister = alan.getOutput();
}


// Loads the pattern register, pattern length, and saved fader locations from the selected bank
void loadPattern(uint8_t loadSlot)
{
  dbprintf("loading bank %u\n", loadSlot);
  for (uint8_t fd = 0; fd < NUM_FADERS; ++fd)
  {
    faderBank[fd]->selectActiveBank(loadSlot);
  }
  dacRegister = alan.getOutput();
  patternPending = -1;
}

