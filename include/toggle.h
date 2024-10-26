#ifndef TOGGLE_DOT_H
#define TOGGLE_DOT_H

#include <MagicButton.h>

// Bit 0 set/clear
extern MagicButton writeHigh;
extern MagicButton writeLow;

enum class toggle_cmd
{
  LESS_OCTAVES,
  MORE_OCTAVES,
  CLEAR_BIT,
  SET_BIT,
  EXIT,
  NO
};

toggle_cmd updateToggle();

#endif