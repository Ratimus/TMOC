#include "TransportParams.h"
#include "ShiftParams.h"


std::array<const uint8_t, NUM_STEP_LENGTHS> STEP_LENGTH_VALS {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 16};

TransportParams::TransportParams():
  load             (0),
  offset_          (0),
  wasReset_        (0),
  drunkStep_       (0),
  inReverse_       (0),
  resetPending_    (0),
  currentBankIdx_  (0),
  newLoadPending_  (0),
  newPatternLoaded_(0),
  shifter_         (ShiftParams()),
  nextPattern_     (currentBankIdx_),
  workingLength    (STEP_LENGTH_VALS.begin() + 6)
{;}



void TransportParams::lengthMINUS()
{
  if (workingLength == STEP_LENGTH_VALS.begin())
  {
    return;
  }
  --workingLength;
  offset_ %= *workingLength;
}


void TransportParams::lengthPLUS()
{
  if (workingLength == (STEP_LENGTH_VALS.end() - 1))
  {
    return;
  }
  ++workingLength;
}


void TransportParams::reAnchor()
{
  offset_ = 0;
}


bool TransportParams::wasNewPatternLoaded() const
{
  return newPatternLoaded_;
}


bool TransportParams::wasReset() const
{
  return wasReset_;
}


uint8_t TransportParams::getLength() const
{
  return *workingLength;
}


int8_t TransportParams::getStep() const
{
  return offset_;
}


uint8_t TransportParams::getSlot() const
{
  return currentBankIdx_;
}


void TransportParams::setNextPattern(uint8_t slot)
{
  newLoadPending_ = true;
  nextPattern_    = slot;
}


void TransportParams::pre_iterate(int8_t steps, bool inPlace)
{
  load = false;
  if (!inPlace)
  {
    // Load new patterns on the downbeat
    if (newLoadPending_ && shifter_.next == 0)
    {
      load      = true;
      wasReset_ = true;
    }
    offset_     = shifter_.next;
  }
  shifter_.getShiftParams(this, steps, inPlace);
}


uint16_t TransportParams::rotateToZero(const uint16_t reg)
{
  return shifter_.rotateToZero(reg, this);
}
