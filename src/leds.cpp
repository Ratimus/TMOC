#include "leds.h"
#include "timers.h"
#include "hwio.h"
#include <RatFuncs.h>


LedController::LedController():
  hw_reg(OutputRegister<uint16_t> (SR_CLK,
                                   SR_DATA,
                                   LED_SR_CS,
                                   regMap)),
  resetBlankTime(100),
  enabled(true)
{;}


void LedController::blinkOut()
{
  enabled = false;
  one_shot(resetBlankTime,
           [this](){enabled = true;});
}


void LedController::setFaderReg()
{
  hw_reg.setReg(alan.getOutput() & faders.getLockByte(), 0);
}


// Updates main horizontal LED array to display current pattern (in
// performance mode) or status (in one of the editing modes)
void LedController::setMainReg()
{
  // Fast blink = LOAD, slower blink = SAVE
  int8_t  slot       = mode.activeSlot();
  uint8_t len        = alan.getLength();
  uint8_t flashTimer = getFlashTimer();

  switch(mode.currentMode())
  {
    case mode_type::PERFORMANCE_MODE:
      // Display the current register/pattern value
      hw_reg.setReg(alan.getOutput(), 1);
      break;

    case mode_type::CHANGE_LENGTH_MODE:
      // Light up the LED corresponding to pattern length. For length > 8,
      // light up all LEDs and dim the active one
      if (len <= 8)
      {
        hw_reg.setReg(0x01 << (len - 1), 1);
      }
      else
      {
        hw_reg.setReg(~(0x01 << (len - 9)), 1);
      }
      break;

    case mode_type::PATTERN_LOAD_MODE:
      // Flash LED corresponding to selected bank
      if (flashTimer & BIT0)
      {
        hw_reg.setReg(0x01 << (uint8_t)slot, 1);
      }
      else
      {
        hw_reg.setReg(0, 1);
      }
      break;

    case mode_type::PATTERN_SAVE_MODE:
      // Flash LED corresponding to selected slot
      if (flashTimer & BIT1)
      {
        hw_reg.setReg(0x01 << (uint8_t)slot, 1);
      }
      else
      {
        hw_reg.setReg(0, 1);
      }
      break;

    default:
      break;
  }
}


void LedController::clock(bool reset)
{
  if (reset)
  {
    // Only do this when you get a clock after a reset
    panelLeds.blinkOut();
  }
  panelLeds.updateAll();
}


void LedController::updateAll()
{
  if (!enabled)
  {
    hw_reg.setReg(0, 0);
    hw_reg.setReg(0, 1);
    hw_reg.clock();
    return;
  }

  setFaderReg();
  setMainReg();
  if (!hw_reg.pending())
  {
    return;
  }
  hw_reg.clock();
}