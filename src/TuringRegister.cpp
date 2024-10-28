
// ------------------------------------------------------------------------
// TuringRegister.cpp
//
// December 2023
// Ryan "Ratimus" Richardson
// ------------------------------------------------------------------------

#include "RatFuncs.h"
#include "TuringRegister.h"
#include "hwio.h"

void TuringRegister::setBit()
{
  stoch_.bitSetPending_   = true;
  stoch_.bitClearPending_ = false;
}


void TuringRegister::clearBit()
{
  stoch_.bitClearPending_ = true;
  stoch_.bitSetPending_   = false;
}


// Returns the current base pattern (i.e. the stored one, not the working one)
uint16_t TuringRegister::getPattern() const
{
  return workingRegister;
}

uint16_t TuringRegister::getReg(uint8_t slot) const
{
  return registersBank[slot];
}


uint8_t TuringRegister::getOutput() const
{
   return (uint8_t)(getPattern() & 0xFF);
}


uint8_t TuringRegister::getLength() const
{
  return transport_.getLength();
}

// Class to hold and manipulate sequencer shift register patterns
TuringRegister::TuringRegister(Stochasticizer& stoch):
    NUM_PATTERNS     (8),
    workingRegister  (0),
    stoch_           (stoch)
{
  // C++ 20 <ranges> is not supported, so we have to do this instead of {enumerate}
  auto reg_iter = registersBank.begin();
  for (uint8_t bk = 0; reg_iter != registersBank.end(); ++bk, ++reg_iter)
  {
    *reg_iter = ~(0x01 << bk);
  }

  workingRegister = *registersBank.begin();
}


// Returns a register with the first [len] bits of [reg] copied into it
// enough times to fill it to the end
uint16_t TuringRegister::norm(const uint16_t reg, const uint8_t len) const
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


uint16_t TransportParams::iterate(uint16_t reg, Stochasticizer stoch)
{
  uint16_t ret = reg;
  if (!wasReset_)
  {
    // Update register
    newPatternLoaded_ = false;
    bool writeVal(bitRead(reg, shifter_.readIdx()));
    ret = (ret << shifter_.leftAmt()) | \
          (ret >> shifter_.rightAmt());
    if (!shifter_.immutable())
    {
      writeVal = stoch.stochasticize(writeVal);
      bitWrite(ret, shifter_.writeIdx(), writeVal);
    }
  }
  return ret;
}


uint16_t TransportParams::loadPattern(const std::array<uint16_t, 8> &bank)
{
  readyToLoad_ = false;
  // Move pattern pointer to selected bank and copy its contents into working register
  currentBankIdx_   = nextPattern_;
  offset_          %= *workingLength;
  newLoadPending_   = false;
  newPatternLoaded_ = true;
  dbprintf("Bank %u: length = %u; step = %u\n", currentBankIdx_, *workingLength, offset_);
  dbprintf("Loaded pattern %u\n", currentBankIdx_);
  return bank[currentBankIdx_];
}


void TuringRegister::iterate(int8_t steps, bool inPlace /*=false*/)
{
  transport_.pre_iterate(steps, inPlace);
  if (transport_.readyToLoad())
  {
    workingRegister = transport_.loadPattern(registersBank);
  }

  workingRegister = transport_.iterate(workingRegister, stoch_);

  if (transport_.newPatternLoaded())
  {
    faders.selectBank(transport_.currentBankIdx());
    dbprintf("loaded fader bank %u\n", transport_.currentBankIdx());
  }

  if (inPlace)
  {
    return;
  }

  // Update all the various outputs
  expandVoltages(getOutput());
  triggers.clock();
  panelLeds.clock(transport_.wasReset());
}


// Selects a new pattern to load on the downbeat. If you select the current
// slot, it will reload it, effectively undoing any changes
void TuringRegister::setNextPattern(uint8_t loadSlot)
{
  loadSlot %= NUM_PATTERNS;
  transport_.setNextPattern(loadSlot);
}


void TuringRegister::rotateToZero()
{
  workingRegister = transport_.rotateToZero(workingRegister);
}


void TuringRegister::reset()
{
  if (transport_.newLoadPending())
  {
    workingRegister = transport_.loadPattern(registersBank);
    transport_.reAnchor();
  }

  if (transport_.resetPending())
  {
    return;
  }
  rotateToZero();
  transport_.flagForReset();
}


void TuringRegister::changeLen(int8_t amt)
{
  if (amt == 0)
  {
    return;
  }

  if (amt > 0)
  {
    transport_.lengthPLUS();
  }
  else
  {
    transport_.lengthMINUS();
  }

  dbprintf("Bank %u: length = %u; step = %u\n",
            transport_.currentBankIdx_,
           *transport_.workingLength,
            transport_.offset_);
}


void TuringRegister::savePattern(uint8_t bankIdx)
{
  // Copy working register into selected bank
  bankIdx %= NUM_PATTERNS;
  uint16_t workingRegCopy = workingRegister;
  rotateToZero();
  registersBank[bankIdx]  = workingRegister;
  workingRegister         = workingRegCopy;

  dbprintf("saved to slot %u\n", bankIdx);
  faders.saveBank(bankIdx);
}



// Make the trigger outputs do something interesting. Ideally, they'll all be related to
// the main pattern register, but still different enough to justify having 8 of them.
// I messed around with a bunch of different bitwise transformations and eventually came
// across a few that I think sound cool.
uint8_t markov(uint8_t seed)
{
  uint8_t ret = 0;
  uint8_t bloops[8]{2, 3, 5, 7, 11, 13, 17, 23};
  for (uint8_t n = 0; n < 8; ++n)
  {
    uint16_t val = ((uint16_t)seed * bloops[n]);
    bitWrite(ret, n, bitRead(val, n) || bitRead(val, n + 8));
    dbprintf("val = %u; ret = %u\n", val, ret);
  }
  return ret;
}
uint8_t TuringRegister::pulseIt()
{
  // Bit 0 from shift register; Bit 1 is !Bit 0
  uint8_t inputReg(static_cast<uint8_t>(workingRegister & 0xFF));
  uint8_t outputReg(inputReg & BIT0);

  inputReg = markov(inputReg);

  outputReg |= (~outputReg << 1) & BIT1;

  outputReg |= ((((inputReg & BIT0) >> 0) & ((inputReg & BIT3) >> 3)) << 2) & BIT2;
  outputReg |= ((((inputReg & BIT2) >> 2) & ((inputReg & BIT7) >> 7)) << 3) & BIT3;

  outputReg |= (((inputReg & BIT1) >> 1) ^ ((inputReg & BIT6) >> 6) << 4) & BIT4;
  outputReg |= (((inputReg & BIT0) >> 0) ^ ((inputReg & BIT4) >> 4) << 5) & BIT5;
  outputReg |= (((inputReg & BIT3) >> 3) ^ ((inputReg & BIT7) >> 7) << 6) & BIT6;
  outputReg |= (((inputReg & BIT2) >> 2) ^ ((inputReg & BIT5) >> 5) << 7) & BIT7;

  printBits(outputReg);
  return outputReg;
}


// "Drunken Walk" algorithm - randomized melodies but notes tend to cluster together
// TODO: Once this is implemented as a sequencer mode, add control over the degree of
// deviation
uint8_t TuringRegister::getDrunkenIndex()
{
  // How big of a step are we taking?
  int8_t rn(random(101));
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
  if (random(2))
  {
    rn *= -1;
  }

  transport_.drunkStep_ += rn;
  if (transport_.drunkStep_ < 0)
  {
    transport_.drunkStep_ += 8;
  }
  else if (transport_.drunkStep_ > 7)
  {
    transport_.drunkStep_ -= 8;
  }

  return transport_.drunkStep_;
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