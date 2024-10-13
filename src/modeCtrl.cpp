#include "modeCtrl.h"
#include "leds.h"

mode_type ModeControl::currentMode()
{
  return currentMode_;
}

int8_t ModeControl::activeSlot()
{
  switch (currentMode_)
  {
    case mode_type::PATTERN_LOAD_MODE:
      return loadSlot_;

    case mode_type::PATTERN_SAVE_MODE:
      return saveSlot_;

    default:
      break;
  }

  return -1;
}


void ModeControl::cancel()
{
  currentMode_ = mode_type::CANCEL;
}

void ModeControl::service()
{
  encoder_.service();
}

ModeCommand ModeControl::click()
{
  switch (currentMode_)
  {
    case mode_type::PERFORMANCE_MODE:
      // Change to "show/edit length" mode
      currentMode_ = mode_type::CHANGE_LENGTH_MODE;
      return {command_enum::CHANGEMODE, 1};

    case mode_type::CHANGE_LENGTH_MODE:
      currentMode_ = mode_type::PERFORMANCE_MODE;
      // Nothing to save or load here since length changes happen "on the fly"
      return {command_enum::CHANGEMODE, 1};

    default:
      return {command_enum::NO_CMD, 0};
  }
}


ModeCommand ModeControl::doubleclick()
{
  switch (currentMode_)
  {
    case mode_type::PERFORMANCE_MODE:
      // Change to "load pattern" mode
      currentMode_ = mode_type::PATTERN_LOAD_MODE;
      return {command_enum::CHANGEMODE, 1};

    case mode_type::PATTERN_LOAD_MODE:
      currentMode_ = mode_type::PERFORMANCE_MODE;
      // Command to load the new pattern
      return {command_enum::LOAD, loadSlot_};

    default:
      return {command_enum::NO_CMD, 0};
  }
}


ModeCommand ModeControl::hold()
{
  switch(currentMode_)
  {
    case mode_type::PERFORMANCE_MODE:
      // Change to "save pattern" mode
      currentMode_ = mode_type::PATTERN_SAVE_MODE;
      return {command_enum::CHANGEMODE, 1};

    case mode_type::PATTERN_SAVE_MODE:
      // Command to save the current pattern
      currentMode_ =  mode_type::PERFORMANCE_MODE;
      return {command_enum::SAVE, saveSlot_};

    default:
      return {command_enum::NO_CMD, 0};
  }
}


ModeCommand ModeControl::right()
{
  switch (currentMode_)
  {
    case mode_type::PERFORMANCE_MODE:
      return {command_enum::STEP, 1};

    case mode_type::CHANGE_LENGTH_MODE:
      return {command_enum::LENGTH, 1};

    case mode_type::PATTERN_LOAD_MODE:
      ++loadSlot_;
      loadSlot_ %= NUM_BANKS;
      return {command_enum::LEDS, 1};

    case mode_type::PATTERN_SAVE_MODE:
      ++saveSlot_;
      saveSlot_ %= NUM_BANKS;
      return {command_enum::LEDS, 1};

    default:
      return {command_enum::NO_CMD, 0};
  }
}


ModeCommand ModeControl::left()
{
  switch (currentMode_)
  {
    case mode_type::PERFORMANCE_MODE:
      return {command_enum::STEP, -1};

    case mode_type::CHANGE_LENGTH_MODE:
      return {command_enum::LENGTH, -1};

    case mode_type::PATTERN_LOAD_MODE:
      --loadSlot_;
      if (loadSlot_ < 0)
      {
        loadSlot_ += NUM_BANKS;
      }
      return {command_enum::LEDS, 1};

    case mode_type::PATTERN_SAVE_MODE:
      --saveSlot_;
      if (saveSlot_ < 0)
      {
        saveSlot_ += NUM_BANKS;
      }
      return {command_enum::LEDS, 1};

    default:
      return {command_enum::NO_CMD, 0};
  }
}


ModeCommand ModeControl::clickhold()
{
  return {command_enum::NO_CMD, 0};
}


ModeCommand ModeControl::shiftleft()
{
  return {command_enum::NO_CMD, 0};
}


ModeCommand ModeControl::shiftright()
{
  return {command_enum::NO_CMD, 0};
}


ModeCommand ModeControl::press()
{
  return {command_enum::NO_CMD, 0};
}


ModeCommand ModeControl::update()
{
  if (currentMode_ == mode_type::CANCEL)
  {
    currentMode_ = mode_type::PERFORMANCE_MODE;
    return {command_enum::LEDS, 1};
  }

  // Check for new events from our encoder and clear the "ready" flag if there are
  switch(encoderInterface_.getEvent())
  {
    case encEvnts::Click:
      return click();

    case encEvnts::DblClick:
      return doubleclick();

    case encEvnts::Hold:
      return hold();

    case encEvnts::Right:
      return right();

    case encEvnts::Left:
      return left();

    case encEvnts::ClickHold:
      return clickhold();

    case encEvnts::ShiftLeft:
      return shiftleft();

    case encEvnts::ShiftRight:
      return shiftright();

    case encEvnts::Press:
      return press();

    case encEvnts::NUM_ENC_EVNTS:
    default:
      return {command_enum::NO_CMD, 0};
  }
}
