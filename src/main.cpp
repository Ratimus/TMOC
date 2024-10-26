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
#include "hwio.h"
#include "setup.h"
#include "toggle.h"
#include "timers.h"

// #define DEBUG_CLOCK

void setup()
{
  setThingsUp();
}


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
      alan.iterate(cmd.val, true);
      break;

    case command_enum::LOAD:
      alan.setNextPattern(cmd.val);
      break;

    case command_enum::SAVE:
      alan.savePattern(cmd.val);
      break;

    case command_enum::LENGTH:
      alan.changeLen(cmd.val);
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
    case toggle_cmd::LESS_OCTAVES:
      faders.lessRange();
      break;

    case toggle_cmd::MORE_OCTAVES:
      faders.moreRange();
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
  if (!newReset())
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
    alan.iterate(-1);
    floating_debug_flag = toggle_cmd::NO;
  }

  if (floating_debug_flag == toggle_cmd::SET_BIT)
  {
    alan.iterate(1);
    floating_debug_flag = toggle_cmd::NO;
  }
#else
  if (newClock())
  {
    alan.iterate((cvB.readRaw() > 2047) ? -1 : 1);
  }
  // else if (clockDown())
  // {
  //   triggers.allOff();
  // }
#endif
}


void loop()
{
  handleMode();
  handleToggle();
  handleReset();
  handleClock();
  panelLeds.updateAll();
}
