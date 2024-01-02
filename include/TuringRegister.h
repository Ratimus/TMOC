#ifndef TURING_REGISTER_DOT_AITCH
#define TURING_REGISTER_DOT_AITCH

#include <Arduino.h>
#include <bitHelpers.h>
#include <MagicButton.h>
#include <ESP32AnalogRead.h>

const uint8_t NUM_STEP_LENGTHS(13);
const uint8_t STEP_LENGTH_VALS[NUM_STEP_LENGTHS]{2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 16};

struct Stochasticizer
{
  float THRESH_LOW;
  float THRESH_HIGH;

  ESP32AnalogRead *pVolts;

  Stochasticizer(
    ESP32AnalogRead * voltage):
    THRESH_LOW(265.0),
    THRESH_HIGH(3125.0),
    pVolts(voltage)
  { ; }

  bool stochasticize(bool startVal)
  {
    float prob(pVolts->readMiliVolts());
    if (prob > THRESH_HIGH)
    {
      return startVal;
    }

    if (prob < THRESH_LOW)
    {
      return !startVal;
    }

    float result(float(random(0, 3135)));
    if (result > prob)
    {
      return !startVal;
    }

    return startVal;
  }
};


class TuringRegister
{
public:
  TuringRegister(
    ESP32AnalogRead *voltage):
    locked_         (0),
    inReverse_      (0),
    offset_         (0),
    resetPending_   (0),
    workingRegister (0),
    stepCountIdx_   (6),
    stoch_(Stochasticizer(voltage)),
    NUM_PATTERNS    (8),
    currentPattern_ (0),
    nextPattern_    (0)
  {
    pLengthArray_ = new uint8_t [NUM_PATTERNS];
    for (uint8_t bk(0); bk < NUM_PATTERNS; ++bk)
    {
      *(pLengthArray_ + bk) = STEP_LENGTH_VALS[stepCountIdx_];
    }
    pLength_      = pLengthArray_;
    pPatternBank  = new uint16_t [NUM_PATTERNS];
    pShiftReg_    = pPatternBank;
    workingRegister = *pShiftReg_;
  }

  ~TuringRegister()
  {
    delete [] pPatternBank;
    pPatternBank = NULL;
  }


  // Shifts register, returns true on downbeat
  bool iterate(int8_t steps)
  {
    if (currentPattern_ != nextPattern_)
    {
      changeBank_(nextPattern_);
    }

    if (resetPending_)
    {
      resetPending_ = false;
      offset_       = steps;
      return true;
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
    if (!locked_)
    {
      writeVal = stoch_.stochasticize(writeVal);
    }

    workingRegister = (workingRegister << leftAmt) | (workingRegister >> rightAmt);
    bitWrite(workingRegister, writeIdx, writeVal);

    bool downBeat(offset_ == 0);

    offset_ += steps;
    if ((offset_ >= *pLength_) || (offset_ <= -*pLength_))
    {
      offset_ = 0;
    }

    return downBeat;
  }

  void reset()
  {
    if (offset_ > 0)
    {
      workingRegister = (workingRegister >> offset_) | (workingRegister << (16 - offset_));
    }
    else if (offset_ < 0)
    {
      workingRegister = (workingRegister << -offset_) | (workingRegister >> (16 + offset_));
    }
    resetPending_ = true;
  }

  void rotateToCurrentStep()
  {
    if (offset_ > 0)
    {
      workingRegister = (workingRegister << offset_) | (workingRegister >> (16 - offset_));
    }
    else if (offset_ < 0)
    {
      workingRegister = (workingRegister >> -offset_) | (workingRegister << (16 + offset_));
    }
  }

  void lengthPLUS()
  {
    if (stepCountIdx_ == NUM_STEP_LENGTHS - 1)
    {
      return;
    }
    ++stepCountIdx_;
    *pLength_ = STEP_LENGTH_VALS[stepCountIdx_];
  }

  void lengthMINUS()
  {
    if (stepCountIdx_ == 0)
    {
      return;
    }
    --stepCountIdx_;
    *pLength_ = STEP_LENGTH_VALS[stepCountIdx_];
  }


  uint16_t getReg()
  {
    return *pShiftReg_;
  }

  uint8_t getOutput() { return (uint8_t)(workingRegister & 0xFF); }
  uint8_t getLength() { return *pLength_; }
  uint8_t getStep()
  {
    if (offset_ < 0)
    {
      return -offset_;
    }
    return offset_;
  }

  uint8_t setNextPattern(uint8_t bankNum)
  {
    bankNum %= NUM_PATTERNS;
    nextPattern_ = bankNum;
    return nextPattern_;
  }

  void preFill(uint16_t fillVal, uint8_t bankNum)
  {
    *(pPatternBank + bankNum) = fillVal;
  }

  void moveToPattern(uint8_t bankIdx)
  {
    // Rotate working register back to step 0 before storing it, but don't set the pending
    // flag if it wasn't already
    bool pr(resetPending_);
    reset();
    resetPending_ = pr;
    *pShiftReg_   = workingRegister;

    // Move pattern pointer to selected bank and copy it into working register
    currentPattern_ = bankIdx % NUM_PATTERNS;
    pShiftReg_      = pPatternBank + currentPattern_;
    workingRegister = *pShiftReg_;

    pLength_ = pLengthArray_ + currentPattern_;
    offset_ %= *pLength_;
    rotateToCurrentStep();
  }

  void overWritePattern(uint8_t bankIdx)
  {
    // Rotate working register to step 0
    bool pr(resetPending_);
    reset();
    resetPending_ = pr;

    // Move pattern pointer to the selected bank and copy working register into it
    currentPattern_ = bankIdx % NUM_PATTERNS;
    pShiftReg_      = pPatternBank + currentPattern_;
    *(pLengthArray_ + currentPattern_) = *pLength_;
    *pShiftReg_     = workingRegister;

    pLength_        = pLengthArray_ + currentPattern_;
    offset_ %= *pLength_;
    rotateToCurrentStep();
  }

protected:
  bool            locked_;
  bool            inReverse_;
  bool            resetPending_;

  uint16_t        workingRegister;
  uint8_t         *pLength_;
  int8_t          offset_;
  uint8_t         stepCountIdx_;
  uint16_t        *pShiftReg_;
  uint8_t         *pLengthArray_;

  Stochasticizer  stoch_;
  const uint8_t   NUM_PATTERNS;
  uint16_t        *pPatternBank;
  uint8_t         currentPattern_;
  uint8_t         nextPattern_;

  void changeBank_(uint8_t bank)
  {
    // Rotate the new pattern to the current step
    if (offset_ > 0)
    {
      *pShiftReg_ = (*pShiftReg_ << offset_) | (*pShiftReg_ >> (16 - offset_));
    }
    else if (offset_ < 0)
    {
      *pShiftReg_ = (*pShiftReg_ >> -offset_) | (*pShiftReg_ << (16 + offset_));
    }
  }
};


#endif
