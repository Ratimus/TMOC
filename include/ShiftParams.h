#ifndef SHIFT_PARAMS_DOT_H
#define SHIFT_PARAMS_DOT_H
#include <Arduino.h>

// Forward declaration so we can declare a function that takes a ptr to one of these
struct TransportParams;

struct ShiftParams
{
  ShiftParams();

  bool    immutable;
  int8_t  next;
  int8_t  readIdx;
  uint8_t writeIdx;
  uint8_t leftAmt;
  uint8_t rightAmt;

  void getShiftParams(TransportParams * pTransport,
                      int8_t steps,
                      bool inPlace);
  uint16_t rotateToZero(const uint16_t workingRegister,
                        TransportParams * pTransport);
};

#endif