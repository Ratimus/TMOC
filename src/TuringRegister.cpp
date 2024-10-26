
// ------------------------------------------------------------------------
// TuringRegister.cpp
//
// December 2023
// Ryan "Ratimus" Richardson
// ------------------------------------------------------------------------

#include "RatFuncs.h"
#include "TuringRegister.h"
#include <Preferences.h>
#include "hwio.h"


Preferences prefs;


// Class to hold and manipulate sequencer shift register patterns
TuringRegister::TuringRegister(Stochasticizer& stoch):
    inReverse_       (0),
    offset_          (0),
    resetPending_    (0),
    workingRegister  (0),
    stepCountIdx_    (6),
    stoch_           (stoch),
    NUM_PATTERNS     (8),
    currentBankIdx_  (0),
    pPatternLength   (&lengthsBank[0]),
    nextPattern_     (currentBankIdx_),
    newLoadPending_  (0),
    drunkStep_       (0),
    wasReset_        (0),
    newPatternLoaded_(0)
{
  for (uint8_t bk(0); bk < NUM_PATTERNS; ++bk)
  {
    lengthsBank[bk] = STEP_LENGTH_VALS[stepCountIdx_];

    // patternBank[bk] = rand();
    patternBank[bk] = ~(0x01 << bk);
  }

  pShiftReg = &patternBank[0];
  pPatternLength = &lengthsBank[0];
  workingRegister = *pShiftReg;
}


TuringRegister::~TuringRegister()
{ ; }


// Returns a register with the first [len] bits of [reg] copied into it
// enough times to fill it to the end
uint16_t TuringRegister::norm(uint16_t reg, uint8_t len) const
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


void TuringRegister::iterate(int8_t steps, bool inPlace /*=false*/)
{
  int8_t next       (0);

  // Shift LEFT on positive steps; RIGHT on negative steps
  uint8_t leftAmt   (0);
  uint8_t rightAmt  (0);

  int8_t  readIdx   (0);
  uint8_t writeIdx  (0);

  // Handle pending reset (if applicable)
  if (resetPending_)
  {
    offset_       = 0;
    wasReset_     = true;
    resetPending_ = false;
  }
  else
  {
    wasReset_     = false;
    if (steps > 0)
    {
      steps       = 1;
      leftAmt     = 1;
      rightAmt    = 15;
      readIdx     = *pPatternLength - 1;
      writeIdx    = 0;
    }
    else
    {
      steps       = -1;
      leftAmt     = 15;
      rightAmt    = 1;
      readIdx     = 8 - *pPatternLength;

      if (readIdx < 0)
      {
        readIdx += 16;
      }
      writeIdx    = 7;
    }
    next = offset_ + steps;
    next %= *pPatternLength;
  }

  // Load new patterns on the downbeat
  if (!inPlace && newLoadPending_ && next == 0)
  {
    loadPattern();
    wasReset_     = true;
  }

  offset_ = next;

  if (!wasReset_)
  {
    // Update register
    newPatternLoaded_ = false;
    bool writeVal(bitRead(workingRegister, readIdx));
    workingRegister = (workingRegister << leftAmt) | \
                      (workingRegister >> rightAmt);
    if (!inPlace)
    {
      writeVal = stoch_.stochasticize(writeVal);
    }

    bitWrite(workingRegister, writeIdx, writeVal);
  }

  if (newPatternLoaded_)
  {
    faders.selectBank(currentBankIdx_);
    dbprintf("loading bank %u\n", currentBankIdx_);
  }

  if (inPlace)
  {
    return;
  }

  // Update all the various outputs
  expandVoltages(getOutput());
  triggers.clock();

  if (wasReset_)
  {
    // Only do this when you get a clock after a reset
    panelLeds.blinkOut();
  }
  panelLeds.updateAll();
}


// Selects a new pattern to load on the downbeat. If you select the current
// slot, it will reload it, effectively undoing any changes
void TuringRegister::setNextPattern(uint8_t loadSlot)
{
  newLoadPending_ = true;
  loadSlot       %= NUM_PATTERNS;
  nextPattern_    = loadSlot;
}


void TuringRegister::returnToZero()
{
  uint8_t leftshift(0);
  uint8_t rightshift(0);
  if (offset_ > 0)
  {
    rightshift  = offset_;
    leftshift   = (16 - offset_);
  }
  else if(offset_ < 0)
  {
    leftshift   = -offset_;
    rightshift  = (16 + offset_);
  }

  if (leftshift == 16)
  {
    leftshift   = 0 ;
  }

  if (rightshift == 16)
  {
    rightshift  = 0;
  }

  workingRegister = (workingRegister >> rightshift) | \
                    (workingRegister << leftshift);
}


void TuringRegister::reset()
{
  if (newLoadPending_)
  {
    loadPattern();
    offset_ = 0;
  }

  if (resetPending_)
  {
    return;
  }
  returnToZero();
  resetPending_ = true;
}


void TuringRegister::changeLen(int8_t amt)
{
  if (amt > 0)
  {
    if (stepCountIdx_ == NUM_STEP_LENGTHS - 1)
    {
      return;
    }
    ++stepCountIdx_;
  }
  else
  {
    if (stepCountIdx_ == 0)
    {
      return;
    }
    --stepCountIdx_;
  }
  pPatternLength = &STEP_LENGTH_VALS[stepCountIdx_];
  offset_       %= *pPatternLength;
}


void TuringRegister::writeToRegister(
  const uint16_t fillVal,
  const uint8_t bankNum)
{
  patternBank[bankNum] = fillVal;
}


void TuringRegister::loadPattern()
{
  // Move pattern pointer to selected bank and copy its contents into working register
  currentBankIdx_   = nextPattern_;
  pShiftReg         = &patternBank[currentBankIdx_];
  workingRegister   = *pShiftReg;
  pPatternLength    = &lengthsBank[currentBankIdx_];
  offset_          %= *pPatternLength;
  newLoadPending_   = false;
  newPatternLoaded_ = true;
}


void TuringRegister::savePattern(uint8_t bankIdx)
{
  // Rotate working register to step 0
  uint16_t workingRegCopy(workingRegister);
  bool pr(resetPending_);
  reset();
  resetPending_   = pr;

  // Copy working register into selected bank
  bankIdx %= NUM_PATTERNS;
  writeToRegister(
    norm(workingRegister, *pPatternLength),
    bankIdx);
  lengthsBank[bankIdx] = *pPatternLength;
  workingRegister = workingRegCopy;

  dbprintf("saved to slot %u\n", bankIdx);
  faders.saveBank(bankIdx);
}

// Make the trigger outputs do something interesting. Ideally, they'll all be related to
// the main pattern register, but still different enough to justify having 8 of them.
// I messed around with a bunch of different bitwise transformations and eventually came
// across a few that I think sound cool.
uint8_t TuringRegister::pulseIt()
{
  // Bit 0 from shift register; Bit 1 is !Bit 0
  uint8_t inputReg(static_cast<uint8_t>(workingRegister & 0xFF));

  uint8_t outputReg(inputReg & BIT0);
  outputReg |= (~outputReg << 1) & BIT1;

  outputReg |= ((((inputReg & BIT0) >> 0) & ((inputReg & BIT3) >> 3)) << 2) & BIT2;
  outputReg |= ((((inputReg & BIT2) >> 2) & ((inputReg & BIT7) >> 7)) << 3) & BIT3;

  outputReg |= (((inputReg & BIT1) >> 1) ^ ((inputReg & BIT6) >> 6) << 4) & BIT4;
  outputReg |= (((inputReg & BIT0) >> 0) ^ ((inputReg & BIT4) >> 4) << 5) & BIT5;
  outputReg |= (((inputReg & BIT3) >> 3) ^ ((inputReg & BIT7) >> 7) << 6) & BIT6;
  outputReg |= (((inputReg & BIT2) >> 2) ^ ((inputReg & BIT5) >> 5) << 7) & BIT7;

  return outputReg;
}


// "Drunken Walk" algorithm - randomized melodies but notes tend to cluster together
// TODO: Once this is implemented as a sequencer mode, add control over the degree of
// deviation
uint8_t TuringRegister::getDrunkenIndex()
{
  // How big of a step are we taking?
  int8_t rn(random(0, 101));
  if (rn > 90)
  {
    rn = 3;
  }
  else if (rn > 70)
  {
    rn = 2;
  }
  else
  {
    rn = 1;
  }

  // Are we stepping forward or backward?
  if (random(0, 2))
  {
    rn *= -1;
  }

  drunkStep_ += rn;
  if (drunkStep_ < 0)
  {
    drunkStep_ += 8;
  }
  else if (drunkStep_ > 7)
  {
    drunkStep_ -= 8;
  }

  return drunkStep_;
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