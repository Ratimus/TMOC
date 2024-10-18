#include "stoch.h"
#include <RatFuncs.h>
Stochasticizer::Stochasticizer(
  ESP32AnalogRead& extLoop,
  ESP32AnalogRead& extCv):
    bitSetPending_(0),
    bitClearPending_(0),
    THRESH_LOW(265.0),
    THRESH_HIGH(3125.0),
    loop(extLoop),
    cv(extCv)
{ ; }


// Coin-toss algorithm; uses knob position to determine likelihood of flipping a
// bit or leaving it untouched
bool Stochasticizer::stochasticize(const bool startVal) const
{
  if (bitSetPending_)
  {
    bitSetPending_ = 0;
    return 1;
  }

  if (bitClearPending_)
  {
    bitClearPending_ = 0;
    return 0;
  }

  uint32_t prob(loop.readMiliVolts());
  uint32_t cvval(cv.readMiliVolts());
  if (prob > THRESH_HIGH)
  {
    if (cv.readMiliVolts() > 500)
    {
      return !startVal;
    }
    return startVal;
  }

  if (prob < THRESH_LOW)
  {
    if (cv.readMiliVolts() > 500)
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
