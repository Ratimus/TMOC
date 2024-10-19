#include "leds.h"
#include "timers.h"
#include "hwio.h"
#include <RatFuncs.h>

// Updates main horizontal LED array to display current pattern (in
// performance mode) or status (in one of the editing modes)

LedController::LedController():
  hw_reg(OutputRegister<uint16_t>  (SR_CLK, SR_DATA, LED_SR_CS, regMap)),
  faderLockStateReg(0xFF),
  resetBlankTime(100),
  enabled(true)
{;}
// Hardware interfaces for 74HC595s

void LedController::blinkOut()
{
  enabled = false;
  one_shot(resetBlankTime, [this](){enabled = true;});
}


void LedController::updateFaderLeds()
{
  // Check whether each individual fader is unlocked; don't light them up unless they are
  for (auto fd(0); fd < NUM_FADERS; ++fd)
  {
    faderBank[fd]->read();
    bitWrite(faderLockStateReg,
             fd,
             faderBank[fd]->getLockState() == LockState::STATE_UNLOCKED);
  }
  hw_reg.setReg(alan.getOutput() & faderLockStateReg, 0);
}


void LedController::updateMainLeds()
{
  if (mode.performing())
  {
    // Display the current register/pattern value
    hw_reg.setReg(alan.getOutput(), 1);
    return;
  }

  if (mode.currentMode() == mode_type::CHANGE_LENGTH_MODE)
  {
    // Light up the LED corresponding to pattern length. For length > 8,
    // light up all LEDs and dim the active one
    uint8_t len(alan.getLength());
    if (len <= 8)
    {
      hw_reg.setReg(0x01 << (len - 1), 1);
    }
    else
    {
      hw_reg.setReg(~(0x01 << (len - 9)), 1);
    }
    return;
  }

  // Fast blink = LOAD, slower blink = SAVE
  uint8_t flashTimer(getFlashTimer());

  if (mode.currentMode() == mode_type::PATTERN_LOAD_MODE)
  {
    int8_t slot(mode.activeSlot());
    if (slot < 0)
    {
      // If we were using exceptions, this would be a good place to throw one
      dbprintf("Holy heck, active slot == -1!\n");
      return;
    }

    // Flash LED corresponding to selected bank
    if (flashTimer & BIT0)
    {
      uint8_t tmp(0x01 << (uint8_t)slot);
      hw_reg.setReg(tmp, 1);
    }
    else
    {
      hw_reg.setReg(0, 1);
    }
    return;
  }

  if (mode.currentMode() == mode_type::PATTERN_SAVE_MODE)
  {
    int8_t slot(mode.activeSlot());
    if (slot < 0)
    {
      dbprintf("Holy heck, active slot == -1!\n");
      // If we were using exceptions, this would be a good place to throw one
      return;
    }

    // Flash LED corresponding to selected slot
    if (flashTimer & BIT1)
    {
      uint8_t tmp(0x01 << (uint8_t)slot);
      hw_reg.setReg(tmp, 1);
    }
    else
    {
      hw_reg.setReg(0, 1);
    }
    return;
  }
}

void LedController::updateAll()
{
  if (!enabled)
  {
    hw_reg.setReg(0, 0);
    hw_reg.setReg(0,1);
    hw_reg.clock();
    return;
  }

  updateFaderLeds();
  updateMainLeds();
  if (!hw_reg.pending())
  {
    return;
  }
  hw_reg.clock();
}