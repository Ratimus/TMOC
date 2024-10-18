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
#ifndef MODE_CTRL_DOT_H
#define MODE_CTRL_DOT_H
#include <ClickEncoderInterface.h>
#include "hw_constants.h"


enum class command_enum {
  STEP,
  LOAD,
  SAVE,
  LENGTH,
  CHANGEMODE,
  LEDS,
  NO_CMD
};

struct ModeCommand
{
  command_enum cmd;
  int8_t val;
};


// Different editing/playback modes for our rotary encoder
enum class mode_type {
  PERFORMANCE_MODE,
  CHANGE_LENGTH_MODE,
  PATTERN_SAVE_MODE,
  PATTERN_LOAD_MODE,
  CANCEL,
  NUM_MODES
};


class ModeControl
{
  // This is the device driver for our rotary encoder
  ClickEncoder encoder_;

  // And this is the interface to the encoder's device driver
  ClickEncoderInterface encoderInterface_;

  int8_t loadSlot_;
  int8_t saveSlot_;

  mode_type currentMode_;

  ModeCommand hold();
  ModeCommand left();
  ModeCommand click();
  ModeCommand right();
  ModeCommand press();
  ModeCommand clickhold();
  ModeCommand shiftleft();
  ModeCommand shiftright();
  ModeCommand doubleclick();

public:
  ModeControl():
    encoder_(ClickEncoder(
      ENC_A,
      ENC_B,
      ENC_SW,
      ENC_STEPS_PER_NOTCH,
      ENC_ACTIVE_LOW)),
    encoderInterface_(ClickEncoderInterface(
      encoder_,
      ENC_SENSITIVITY))
  {
    ;
  }

  void cancel();
  void service();
  int8_t activeSlot();
  ModeCommand update();
  mode_type currentMode();
  bool performing();
};


#endif







