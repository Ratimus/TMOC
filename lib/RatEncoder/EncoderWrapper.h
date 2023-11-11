#ifndef EncoderWrapper_H
#define EncoderWrapper_H

#include <Arduino.h>
#include "ClickEncoderInterface.h"
#include <RatRotaryEvent.h>

namespace Menu
{
class EncoderWrapper : public menuIn
{
private:

  ClickEncoderInterface &EI;
  RatRotaryEvent &REI;
  encEvnts _evt;

public:

  EncoderWrapper(
    ClickEncoderInterface &EncoderInterface,
    RatRotaryEvent &Rotary) :
  EI(EncoderInterface),
  REI(Rotary),
  _evt(encEvnts::NUM_ENC_EVNTS)
  { update(); }

  inline void update()
  {
    _evt = EI.getEvent();
  }

  int available(void)
  {
    return peek() != -1;
  }

  int peek(void)
  {
    return _evt;
  }

  int read()
  {
    update();
    return peek();
  }

  void flush()
  {
    EI.flush();
  }

  size_t write(uint8_t v)
  {
    return 1;
  }
};
}//namespace Menu

#endif /* EncoderWrapper_h */
