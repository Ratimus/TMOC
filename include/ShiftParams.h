#ifndef SHIFT_PARAMS_DOT_H
#define SHIFT_PARAMS_DOT_H
#include <Arduino.h>

// Forward declaration so we can declare a function that takes a ptr to one of these
class TransportParams;

class ShiftParams
{
  bool    immutable_;
  int8_t  next_;
  int8_t  readIdx_;
  uint8_t writeIdx_;
  uint8_t leftAmt_;
  uint8_t rightAmt_;

public:
  ShiftParams();

  void      getShiftParams(TransportParams & transport,
                           const int8_t steps,
                           const bool inPlace);

  uint16_t  rotateToZero(const uint16_t workingRegister,
                         const int8_t step);

  int8_t    next()      const;
  int8_t    readIdx()   const;
  uint8_t   writeIdx()  const;
  uint8_t   leftAmt()   const;
  uint8_t   rightAmt()  const;
  bool      immutable() const;
};

#endif