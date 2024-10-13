#ifndef TOGGLE_DOT_H
#define TOGGLE_DOT_H

#include <MagicButton.h>

enum class toggle_cmd
{
  MORE_OCTAVES,
  LESS_OCTAVES,
  CLEAR_BIT,
  SET_BIT,
  EXIT,
  NO
};

toggle_cmd updateToggle();

#endif