
// ------------------------------------------------------------------------
// TuringRegister.cpp
//
// December 2023
// Ryan "Ratimus" Richardson
// ------------------------------------------------------------------------

#include "RatFuncs.h"
// #include <Arduino.h>
// #include <bitHelpers.h>
// #include <MagicButton.h>
// #include <Preferences.h>
// #include <ESP32AnalogRead.h>
#include "TuringRegister.h"
#include <Preferences.h>
Preferences prefs;


Stochasticizer :: Stochasticizer(
  ESP32AnalogRead * voltage,
  ESP32AnalogRead * cv):
    THRESH_LOW(265.0),
    THRESH_HIGH(3125.0),
    pVolts(voltage),
    pCV(cv)
{ ; }


// Coin-toss algorithm; uses knob position to determine likelihood of flipping a
// bit or leaving it untouched
bool Stochasticizer :: stochasticize(bool startVal)
{
  float prob(pVolts->readMiliVolts());
  if (prob > THRESH_HIGH)
  {
    if (pCV->readMiliVolts() > 500)
    {
      return !startVal;
    }
    return startVal;
  }

  if (prob < THRESH_LOW)
  {
    if (pCV->readMiliVolts() > 500)
    {
      return startVal;
    }
    return !startVal;
  }

  float result(float(random(0, 3135)));
  if (result > prob)
  {
    return !startVal;
  }

  return startVal;
}


// Class to hold and manipulate sequencer shift register patterns
TuringRegister :: TuringRegister(
  ESP32AnalogRead *voltage,
  ESP32AnalogRead *cv):
    inReverse_      (0),
    offset_         (0),
    resetPending_   (0),
    workingRegister (0),
    stepCountIdx_   (6),
    stoch_(Stochasticizer(voltage, cv)),
    NUM_PATTERNS    (8),
    currentBankIdx_ (0)
{
  pLengthArray_   = new uint8_t [NUM_PATTERNS];
  for (uint8_t bk(0); bk < NUM_PATTERNS; ++bk)
  {
    *(pLengthArray_ + bk) = STEP_LENGTH_VALS[stepCountIdx_];
  }
  pLength_        = pLengthArray_;
  pPatternBank    = new uint16_t [NUM_PATTERNS];
  pShiftReg_      = pPatternBank;
  workingRegister = *pShiftReg_;
}


TuringRegister :: ~TuringRegister()
{
  delete [] pPatternBank;
  pPatternBank = NULL;
}


// Returns a register with the first [len] bits of [reg] copied into it
// enough times to fill it to the end
uint16_t TuringRegister :: norm(uint16_t reg, uint8_t len)
{
  uint16_t ret(0);
  uint8_t idx(0);
  uint8_t absIdx(0);
  while (true)
  {
    idx = 0;
    while (idx < len)
    {
      ret |= (reg & (0x01 << idx));
      ++idx;
      ++absIdx;
    }

    if (absIdx >= 16)
    {
      break;
    }
    ret <<= len;
  }

  return ret;
}


// Shifts register, returns current step [i.e. prior to advancing]
uint8_t TuringRegister :: iterate(int8_t steps)
{
  // dbprintf("%s shift %u steps %s\n",
  //          passive ? "passive" : "active",
  //          steps > 0 ? steps : -steps,
  //          steps > 0 ? "forward" : "backward");

  if (resetPending_)
  {
    resetPending_ = false;
    offset_       = steps;
    return 0;
  }

  uint8_t leftAmt;
  uint8_t rightAmt;

  int8_t  readIdx;
  uint8_t writeIdx;

  if (steps > 0)  // Advance pattern
  {
    leftAmt   = 1;
    rightAmt  = 15;
    readIdx   = *pLength_ - 1;
    writeIdx  = 0;
  }
  else
  {
    leftAmt   = 15;
    rightAmt  = 1;
    readIdx   = 8 - *pLength_;

    if (readIdx < 0)
    {
      readIdx += 16;
    }
    writeIdx = 7;
  }

  bool writeVal(bitRead(workingRegister, readIdx));
  workingRegister = (workingRegister << leftAmt) | \
                    (workingRegister >> rightAmt);
  writeVal = stoch_.stochasticize(writeVal);

  bitWrite(workingRegister, writeIdx, writeVal);
  uint8_t retVal(offset_);

  offset_ += steps;
  if ((offset_ >= *pLength_) || (offset_ <= -*pLength_))
  {
    offset_ = 0;
  }

  return retVal;
}


// Rotates the working register back to step 0
void TuringRegister :: reset()
{
  offset_ = -offset_;
  rotateToCurrentStep();
  offset_ = -offset_;
  resetPending_ = true;
}


void TuringRegister :: rotateToCurrentStep()
{
  if (offset_ > 0)
  {
    workingRegister = (workingRegister << offset_) | \
                      (workingRegister >> (16 - offset_));
  }
  else if (offset_ < 0)
  {
    workingRegister = (workingRegister >> -offset_) | \
                      (workingRegister << (16 + offset_));
  }
}


void TuringRegister :: lengthPLUS()
{
  if (stepCountIdx_ == NUM_STEP_LENGTHS - 1)
  {
    return;
  }
  ++stepCountIdx_;
  *pLength_ = STEP_LENGTH_VALS[stepCountIdx_];
}


void TuringRegister :: lengthMINUS()
{
  if (stepCountIdx_ == 0)
  {
    return;
  }
  --stepCountIdx_;
  *pLength_ = STEP_LENGTH_VALS[stepCountIdx_];
}


void TuringRegister :: writeBit(uint8_t idx, bool bitVal)
{
  bitWrite(workingRegister, idx, bitVal);
}



void TuringRegister :: writeToRegister(uint16_t fillVal, uint8_t bankNum)
{
  *(pPatternBank + bankNum) = fillVal;
}


void TuringRegister :: loadPattern(uint8_t bankIdx, bool saveFirst)
{
  // Rotate working register [offset_] steps RIGHT back to step 0
  bool pr(resetPending_);

  reset();

  // Clear this flag if it wasn't already set
  resetPending_   = pr;

  if (saveFirst)
  {
    *pShiftReg_ = workingRegister;
  }

  // Move pattern pointer to selected bank and copy its contents into working register
  currentBankIdx_ = bankIdx % NUM_PATTERNS;
  pShiftReg_      = pPatternBank + currentBankIdx_;
  workingRegister = *pShiftReg_;

  pLength_ = pLengthArray_ + currentBankIdx_;
  offset_ %= *pLength_;
  rotateToCurrentStep();
}


void TuringRegister :: savePattern(uint8_t bankIdx)
{
  // Rotate working register to step 0
  uint16_t workingRegCopy(workingRegister);
  bool pr(resetPending_);
  reset();
  resetPending_   = pr;

  // Copy working register into selected bank
  bankIdx %= NUM_PATTERNS;
  writeToRegister(norm(workingRegister, *pLength_), bankIdx);
  *(pLengthArray_ + bankIdx) = *pLength_;
  workingRegister = workingRegCopy;

  dbprintf("saved to slot %u\n", bankIdx);
  printBits(*(pPatternBank + bankIdx));
}


// TODO: incorporate this stuff:

// void savePatternToFlash(uint8_t slot)
// {
//   char lclbuf[8];
//   sprintf(lclbuf, "reg%u", slot);
//   prefs.putUShort(lclbuf, 0);

//   int16_t min;
//   int16_t max;
//   int16_t lockVal;
// }

// // save settings to eeprom
// void savesetup(void)
// {
//     // https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/
//   prefs.begin("setup", false);  // Read-only = false
//    ...
//   prefs.end();
//   Serial.println("Settings saved");
//   return proceed;
// }


// void loadPrefs(uint8_t vidx)
// {
// prefs.begin("setup", true);  // Read-only = true
//  ...
// prefs.end();
// }