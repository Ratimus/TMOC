#include "setup.h"
#include "toggle.h"
#include "hw_constants.h"

// Bit 0 set/clear
MagicButton writeHigh(TOGGLE_UP, 1, 1);
MagicButton writeLow(TOGGLE_DOWN, 1, 1);

toggle_cmd updateToggle()
{
  // Double-click to change fader octave range (1 to 3 octaves)
  ButtonState clickies[]
  {
    writeLow.readAndFree(),
    writeHigh.readAndFree()
  };

  if (clickies[0] == ButtonState::DoubleClicked)
  {
    return toggle_cmd::LESS_OCTAVES;
  }

  if (clickies[1] == ButtonState::DoubleClicked)
  {
    return toggle_cmd::MORE_OCTAVES;
  }

  // Single click to set/clear BIT0
  if (clickies[0] == ButtonState::Clicked
   || clickies[0] == ButtonState::Held)
  {
    return toggle_cmd::CLEAR_BIT;
  }

  if (clickies[1] == ButtonState::Clicked
   || clickies[1] == ButtonState::Held)
  {
    if (mode.performing())
    {
      return toggle_cmd::SET_BIT;
    }

    return toggle_cmd::EXIT;
  }

  return toggle_cmd::NO;
}
