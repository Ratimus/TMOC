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
#include "toggle.h"
#include "timers.h"

#define DEBUG_CLOCK

#define RESET_FLAG 1
#define CLOCK_FLAG 0

void setup()
{
  setThingsUp();
}

void savePattern(uint8_t saveSlot);
void loadNewFaderBank();
void onClock(int8_t stepAmount);
void handleOutputs();
void handleTriggers();

void handleMode()
{
  ModeCommand cmd(mode.update());
  if(cmd.cmd == command_enum::NO_CMD)
  {
    return;
  }

  switch(cmd.cmd)
  {
    case command_enum::CHANGEMODE:
      break;

    case command_enum::STEP:
      if (cmd.val < 0)
      {
        alan.iterate(-1);
      }
      else
      {
        alan.iterate(1);
      }
      // This only happens in mode_type::PERFORMANCE_MODE, so no need to check
      panelLeds.updateAll();
      break;

    case command_enum::LOAD:
      alan.setNextPattern(cmd.val);
      break;

    case command_enum::SAVE:
      savePattern(cmd.val);
      break;

    case command_enum::LENGTH:
      if (cmd.val < 0)
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

void handleToggle()
{
  #ifdef DEBUG_CLOCK
  return;
  #else
  toggle_cmd cmd(updateToggle());
  switch(cmd)
  {
    case toggle_cmd::MORE_OCTAVES:
      if (OctaveRange > 1)
      {
        --OctaveRange;
        setRange(OctaveRange);
      }
      break;

    case toggle_cmd::LESS_OCTAVES:
      if (OctaveRange < 3)
      {
        ++OctaveRange;
        setRange(OctaveRange);
      }
      break;

    case toggle_cmd::CLEAR_BIT:
      alan.clearBit();
      break;

    case toggle_cmd::SET_BIT:
      alan.setBit();
      break;

    case toggle_cmd::EXIT:
      mode.cancel();
      break;

    case toggle_cmd::NO:
    default:
      break;
  }
  #endif
}

#ifdef DEBUG_CLOCK
toggle_cmd floating_debug_flag(toggle_cmd::NO);
#endif

void handleReset()
{

  #ifdef DEBUG_CLOCK
  floating_debug_flag = updateToggle();
  if (floating_debug_flag == toggle_cmd::LESS_OCTAVES)
  {
    alan.reset();
    floating_debug_flag = toggle_cmd::NO;
    return;
  }
  #else
  if (!gates.readRiseFlag(RESET_FLAG))
  {
    return;
  }

  alan.reset();
  #endif
}

void handleClock()
{
  #ifdef DEBUG_CLOCK
  if (floating_debug_flag == toggle_cmd::CLEAR_BIT)
  {
    dbprintf("!KCOLC\n");
    onClock(-1);
    floating_debug_flag = toggle_cmd::NO;
  }

  if (floating_debug_flag == toggle_cmd::SET_BIT)
  {
    dbprintf("CLOCK!\n");
    onClock(1);
    floating_debug_flag = toggle_cmd::NO;
  }
  #else
  if (gates.readRiseFlag(CLOCK_FLAG))
  {
    dbprintf("CLOCK!\n");
    // Single clicks on the toggle will set/clear the next bit, but we also want to
    // be able to hold it down and write or clear the entire register
    onClock((cvB.readRaw() > 2047) ? -1 : 1, resetCleared);
  }
  else if (gates.readFallFlag(CLOCK_FLAG))
  {
    triggers.allOff();
  }
  #endif
}

void handleOutputs()
{
  // Update all the various outputs
  expandVoltages(alan.getOutput());
}

void handleLeds()
{
  panelLeds.updateAll();
}


// TODO: I updated my MagicButton class a while ago to only register a 'Held' state after you
// release, so I need to bring that back as an option if I want it to function that way

void loop()
{
  handleMode();
  handleToggle();
  handleReset();
  handleClock();
  handleLeds();
  serviceRunList();
}


// Got an external clock pulse? Do the thing
void onClock(int8_t stepAmount)
{
  // Iterate the sequencer to the next state
  alan.iterate(stepAmount);

  if (alan.wasReset())
  {
    // Only do this when you get a clock after a reset
    panelLeds.blinkOut();
  }

  if (alan.newPattern())
  {
    loadNewFaderBank();
  }

  handleOutputs();
  handleTriggers();
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
}


// Loads the pattern register, pattern length, and saved fader locations from the selected bank
void loadNewFaderBank()
{
  uint8_t loadSlot = alan.getSlot();

  dbprintf("loading bank %u\n", loadSlot);
  for (uint8_t fd = 0; fd < NUM_FADERS; ++fd)
  {
    faderBank[fd]->selectActiveBank(loadSlot);
  }
}

