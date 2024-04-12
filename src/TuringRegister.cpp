
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


bool Stochasticizer :: stochasticize(bool startVal)
{
  // TODO: add this stuff some day, maybe?
  // int decide = random(1, 511);  // and a random integer (1:510)
  // int Turing(pVolts->readMiliVolts());
  // static float someConst(3.1412 / 1024.0);
  // int TuringCurve = floor(512.0 * (1.0 - cos(((float)Turing) * someConst)));

  // if (Turing >= 512)
  // {
  //   // Here, we're in the right-hand side of the TURING control
  //   if (decide < 1024 - TuringCurve)
  //   {
  //     // compare the random variable 'decide' with 1024-Turing Curve
  //     // if it's smaller, randomise gate 0
  //     gates[0] = randgate;
  //   }
  //   else
  //   {
  //     // feedback the bit from the selected point in the shift register into gate 0
  //     gates[0] = (gatecarry);
  //   }
  // }
  // else
  // {
  //   // Here, we're in the left-hand side of the TURING control
  //   if (decide < TuringCurve)
  //   {
  //     // compare the random variable 'decide' with Turing Curve
  //     // if it's smaller, randomise gate 0
  //     gates[0] = randgate;
  //   }
  //   else
  //   {
  //     // feedback the complement of the bit from the selected point in the shift register into gate 0
  //     gates[0] = not(gatecarry);
  //   }
  // }

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
uint8_t TuringRegister :: iterate(int8_t steps, bool passive)
{
  if (!passive)
  {
    if (anchorBit0_)
    {
      anchorBit0_   = false;
      offset_       = 0;
    }

    if (resetPending_)
    {
      resetPending_ = false;
      offset_       = steps;
      return 0;
    }
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

  bool writeVal(stoch_.stochasticize(bitRead(workingRegister, readIdx)));
  workingRegister = (workingRegister << leftAmt) | \
                    (workingRegister >> rightAmt);

  if (passive)
  {
    return offset_;
  }

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


void TuringRegister :: quietRotate(int8_t steps)
{
  int8_t dir(steps > 0 ? 1 : -1);
  for (auto step(0); step < steps; ++ step)
  {
    iterate(dir, true);
  }
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
//   for (uint8_t vidx(0); vidx < 4; ++vidx)
//   {
//     Voice *pVoice = &voice[vidx];
//     switch(vidx)
//     {
//       case 0:
//         prefs.putUChar("cv_mode0", pVoice->cv_mode.D);
//         prefs.putUChar("sample0", pVoice->sample.D);
//         prefs.putUChar("mix0", pVoice->mix.D);
//         prefs.putChar("pitch0", pVoice->pitch.D);
//         prefs.putUChar("decay0", pVoice->decay.D);
//         prefs.putUChar("envShape0", pVoice->envShape.D);
//         prefs.putBool("choke0", pVoice->choke.D);
//         break;
//       case 1:
//         prefs.putUChar("cv_mode1", pVoice->cv_mode.D);
//         prefs.putUChar("sample1", pVoice->sample.D);
//         prefs.putUChar("mix1", pVoice->mix.D);
//         prefs.putChar("pitch1", pVoice->pitch.D);
//         prefs.putUChar("decay1", pVoice->decay.D);
//         prefs.putUChar("envShape1", pVoice->envShape.D);
//         break;
//       case 2:
//         prefs.putUChar("cv_mode2", pVoice->cv_mode.D);
//         prefs.putUChar("sample2", pVoice->sample.D);
//         prefs.putUChar("mix2", pVoice->mix.D);
//         prefs.putChar("pitch2", pVoice->pitch.D);
//         prefs.putUChar("decay2", pVoice->decay.D);
//         prefs.putUChar("envShape2", pVoice->envShape.D);
//         prefs.putBool("choke2", pVoice->choke.D);
//         break;
//       case 3:
//         prefs.putUChar("cv_mode3", pVoice->cv_mode.D);
//         prefs.putUChar("sample3", pVoice->sample.D);
//         prefs.putUChar("mix3", pVoice->mix.D);
//         prefs.putChar("pitch3", pVoice->pitch.D);
//         prefs.putUChar("decay3", pVoice->decay.D);
//         prefs.putUChar("envShape3", pVoice->envShape.D);
//         break;
//       default:
//         break;
//     }
//   }
//   prefs.end();
//   Serial.println("Settings saved");
//   return proceed;
// }


// void loadPrefs(uint8_t vidx)
// {
// prefs.begin("setup", true);  // Read-only = true
// Voice *pVoice = &voice[vidx];
// uint8_t cv_mode, mix, pitch, decay;
// switch(vidx)
// {
//   case 0:
//     pVoice->setDefaults(
//       prefs.getUChar("sample0", 0),
//       prefs.getUChar("mix0", 100),
//       prefs.getUChar("decay0", 100),
//       prefs.getChar("pitch0", 0),
//       prefs.getUChar("cv_mode0", NONE),
//       prefs.getUChar("envShape0", 2),
//       prefs.getBool("choke0", 1));
//     break;
//   case 1:
//     pVoice->setDefaults(
//       prefs.getUChar("sample1", 3),
//       prefs.getUChar("mix1", 100),
//       prefs.getUChar("decay1", 100),
//       prefs.getChar("pitch1", 0),
//       prefs.getUChar("cv_mode1", NONE),
//       prefs.getUChar("envShape1", 2),
//       false);
//     break;
//   case 2:
//     pVoice->setDefaults(
//       prefs.getUChar("sample2", 7),
//       prefs.getUChar("mix2", 100),
//       prefs.getUChar("decay2", 100),
//       prefs.getChar("pitch2", 0),
//       prefs.getUChar("cv_mode2", NONE),
//       prefs.getUChar("envShape2", 2),
//       prefs.getBool("choke2", 1));
//     break;
//   case 3:
//     pVoice->setDefaults(
//       prefs.getUChar("sample3", 17),
//       prefs.getUChar("mix3", 100),
//       prefs.getUChar("decay3", 100),
//       prefs.getChar("pitch3", 0),
//       prefs.getUChar("cv_mode3", NONE),
//       prefs.getUChar("envShape3", 2),
//       false);
//     break;
//   default:
//     break;
// }
// prefs.end();
// Serial.printf("Loaded Channel %d with sample %d (%s)\n", vidx, pVoice->sample.Q, pVoice->pSample->sname);
// }