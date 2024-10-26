#ifndef TOGGLE_DOT_H
#define TOGGLE_DOT_H

#include <MagicButton.h>

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