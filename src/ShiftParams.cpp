#include "ShiftParams.h"
#include "TransportParams.h"


ShiftParams::ShiftParams():
  next(0),
  leftAmt(0),
  rightAmt(0),
  readIdx(0),
  writeIdx(0),
  immutable(0)
{ ; }

void ShiftParams::getShiftParams(TransportParams * pTransport,
                                 int8_t steps,
                                 bool inPlace)
{
  immutable = inPlace;

  // Handle pending reset (if applicable)
  if (pTransport->resetPending_ && !immutable)
  {
    pTransport->offset_       = 0;
    pTransport->wasReset_     = true;
    pTransport->resetPending_ = false;
    // dbprintln("reset handled");
  }

  pTransport->wasReset_       = false;
  if (steps > 0)
  {
    steps       = 1;
    leftAmt     = 1;
    rightAmt    = 15;
    readIdx     = 1 + *pTransport->workingLength;
    writeIdx    = 0;
  }
  else
  {
    steps       = -1;
    leftAmt     = 15;
    rightAmt    = 1;
    readIdx     = 8 - *pTransport->workingLength;

    if (readIdx < 0)
    {
      readIdx  += 16;
    }
    writeIdx    = 7;
  }
  next = pTransport->offset_ + steps;
  next %= *pTransport->workingLength;
}


uint16_t ShiftParams::rotateToZero(const uint16_t workingRegister,
                                   TransportParams * pTransport)
{
  if (pTransport->offset_ > 0)
  {
    rightAmt  = pTransport->offset_;
    leftAmt   = (16 - pTransport->offset_);
  }
  else if(pTransport->offset_ < 0)
  {
    leftAmt   = -pTransport->offset_;
    rightAmt  = (16 + pTransport->offset_);
  }

  if (leftAmt == 16)
  {
    leftAmt   = 0 ;
  }

  if (rightAmt == 16)
  {
    rightAmt  = 0;
  }

  return (workingRegister >> rightAmt) | \
         (workingRegister << leftAmt);
}
