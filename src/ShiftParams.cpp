#include "ShiftParams.h"
#include "TransportParams.h"
#include <memory>

ShiftParams::ShiftParams():
  next_      (0),
  leftAmt_   (0),
  rightAmt_  (0),
  readIdx_   (0),
  writeIdx_  (0),
  immutable_ (0)
{ ; }

void ShiftParams::getShiftParams(TransportParams & transport,
                                 const int8_t steps,
                                 const bool inPlace)
{
  immutable_ = inPlace;
  if (transport.resetPending() && !immutable_)
  {
    // Handle pending reset (if applicable)
    transport.reset();
    next_         = 0;
    // dbprintln("reset handled");
    return;
  }

  int8_t shiftAmt = 1;
  if (steps > 0)
  {
    leftAmt_      = 1;
    rightAmt_     = 15;
    readIdx_      = 1 + transport.getLength();
    writeIdx_     = 0;
  }
  else
  {
    shiftAmt      = -1;
    leftAmt_      = 15;
    rightAmt_     = 1;
    readIdx_      = 8 - transport.getLength();

    if (readIdx_ < 0)
    {
      readIdx_   += 16;
    }
    writeIdx_     = 7;
  }
  next_ = (transport.getStep() + shiftAmt) % transport.getLength();
}


int8_t ShiftParams::next() const
{
  return next_;
}


int8_t ShiftParams::readIdx() const
{
  return readIdx_;
}


uint8_t ShiftParams::writeIdx() const
{
  return writeIdx_;
}


uint8_t ShiftParams::leftAmt() const
{
  return leftAmt_;
}


uint8_t ShiftParams::rightAmt() const
{
  return rightAmt_;
}


bool ShiftParams::immutable() const
{
  return immutable_;
}


uint16_t ShiftParams::rotateToZero(const uint16_t workingRegister,
                                   const int8_t step)
{
  if (step > 0)
  {
    rightAmt_  = step;
    leftAmt_   = (16 - step);
  }
  else if(step < 0)
  {
    leftAmt_   = -step;
    rightAmt_  = (16 + step);
  }

  if (leftAmt_ == 16)
  {
    leftAmt_   = 0 ;
  }

  if (rightAmt_ == 16)
  {
    rightAmt_  = 0;
  }

  return (workingRegister >> rightAmt_) | \
         (workingRegister << leftAmt_);
}
