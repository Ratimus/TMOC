#include "stoch.h"

Stochasticizer::Stochasticizer(
  ESP32AnalogRead& extLoop,
  ESP32AnalogRead& extCv):
    THRESH_LOW(265.0),
    THRESH_HIGH(3125.0),
    loop(extLoop),
    cv(extCv)
{ ; }


// Coin-toss algorithm; uses knob position to determine likelihood of flipping a
// bit or leaving it untouched
bool Stochasticizer::stochasticize(const bool startVal) const
{
  float prob(loop.readMiliVolts());
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
