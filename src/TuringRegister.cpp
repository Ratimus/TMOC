
// ------------------------------------------------------------------------
// TuringRegister.cpp
//
// December 2023
// Ryan "Ratimus" Richardson
// ------------------------------------------------------------------------

#include "RatFuncs.h"
#include "TuringRegister.h"
#include <Preferences.h>

Preferences prefs;


// Class to hold and manipulate sequencer shift register patterns
TuringRegister::TuringRegister(Stochasticizer& stoch):
    inReverse_      (0),
    offset_         (0),
    resetPending_   (0),
    workingRegister (0),
    stepCountIdx_   (6),
    stoch_          (stoch),
    NUM_PATTERNS    (8),
    currentBankIdx_ (0),
    pPatternLength  (&lengthsBank[0]),
    nextPattern_    (currentBankIdx_),
    newLoadPending_ (0),
    drunkStep_      (0)
{
  lengthsBank.reserve(NUM_PATTERNS);
  patternBank.reserve(NUM_PATTERNS);

  for (uint8_t bk(0); bk < NUM_PATTERNS; ++bk)
  {
    lengthsBank.push_back(STEP_LENGTH_VALS[stepCountIdx_]);
    patternBank.push_back(rand());
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


// Shifts register, returns current step [i.e. prior to advancing]
uint8_t TuringRegister::iterate(int8_t steps)
{
  // Handle pending reset (if applicable)
  bool resat(false);
  if (resetPending_)
  {
    resetPending_ = false;
    returnToZero();
    offset_       = 0;
    resat = true;
  }

  // Load new patterns on the downbeat
  if (newLoadPending_ && offset_ == 0)
  {
    loadPattern();
  }

  if (resat)
  {
    dbprintf("Resat to step %d\n", offset_);
    return 0;
  }

  // Shift LEFT on positive steps; RIGHT on negative steps
  uint8_t leftAmt;
  uint8_t rightAmt;

  int8_t  readIdx;
  uint8_t writeIdx;
  if (steps > 0)  // Advance pattern
  {
    leftAmt   = 1;
    rightAmt  = 15;
    readIdx   = *pPatternLength - 1;
    writeIdx  = 0;
  }
  else
  {
    leftAmt   = 15;
    rightAmt  = 1;
    readIdx   = 8 - *pPatternLength;

    if (readIdx < 0)
    {
      readIdx += 16;
    }
    writeIdx = 7;
  }

  // Update register
  bool writeVal(bitRead(workingRegister, readIdx));
  workingRegister = (workingRegister << leftAmt) | \
                    (workingRegister >> rightAmt);
  //writeVal = stoch_.stochasticize(writeVal);
  //debug

  bitWrite(workingRegister, writeIdx, writeVal);
  uint8_t retVal(offset_);

  offset_ += steps;
  if ((offset_ >= *pPatternLength) || (offset_ <= -*pPatternLength))
  {
    offset_ = 0;
  }

  dbprintf("Step %d\n", offset_);
  return offset_;
}


// Selects a new pattern to load on the downbeat. If you select the current
// slot, it will reload it, effectively undoing any changes
bool TuringRegister::setNextPattern(uint8_t loadSlot)
{
  newLoadPending_ = true;
  loadSlot %= NUM_PATTERNS;
  nextPattern_ = loadSlot;
  return newLoadPending_;
}


void TuringRegister::returnToZero()
{
  uint8_t rightshift(0);
  uint8_t leftshift(0);
  if (offset_ > 0)
  {
    rightshift = offset_;
    leftshift = (16 - offset_);
  }
  else if(offset_ < 0)
  {
    leftshift = -offset_;
    rightshift = (16 + offset_);
  }

  if (leftshift == 16)
  {
    leftshift = 0 ;
  }

  if (rightshift == 16)
  {
    rightshift = 0;
  }

  workingRegister = (workingRegister >> rightshift) | \
                    (workingRegister << leftshift);
}


void TuringRegister::reset()
{
  while (offset_)
  {
    if (offset_ < 0)
    {
      iterate(1);
    }
    else
    {
      iterate(-1);
    }
  }
  resetPending_ = true;
}


void TuringRegister::lengthPLUS()
{
  if (stepCountIdx_ == NUM_STEP_LENGTHS - 1)
  {
    return;
  }
  ++stepCountIdx_;
  pPatternLength = &STEP_LENGTH_VALS[stepCountIdx_];
}


void TuringRegister::lengthMINUS()
{
  if (stepCountIdx_ == 0)
  {
    return;
  }
  --stepCountIdx_;
  pPatternLength = &STEP_LENGTH_VALS[stepCountIdx_];
  offset_       %= *pPatternLength;
}


void TuringRegister::writeBit(const uint8_t idx, const bool bitVal)
{
  bitWrite(workingRegister, idx, bitVal);
}


void TuringRegister::writeToRegister(const uint16_t fillVal, const uint8_t bankNum)
{
  patternBank[bankNum] = fillVal;
}


void TuringRegister::loadPattern()
{
  // Move pattern pointer to selected bank and copy its contents into working register
  currentBankIdx_ = nextPattern_;
  pShiftReg       = &patternBank[currentBankIdx_];
  workingRegister = *pShiftReg;
  pPatternLength  = &lengthsBank[currentBankIdx_];
  offset_        %= *pPatternLength;
  newLoadPending_ = false;
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
  writeToRegister(norm(workingRegister, *pPatternLength), bankIdx);
  lengthsBank[bankIdx] = *pPatternLength;
  workingRegister = workingRegCopy;

  dbprintf("saved to slot %u\n", bankIdx);
  printBits(lengthsBank[bankIdx]);
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