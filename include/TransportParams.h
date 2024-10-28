#ifndef TRANSPORT_PARAMS_DOT_H
#define TRANSPORT_PARAMS_DOT_H
#include <Arduino.h>
#include "stoch.h"
#include "ShiftParams.h"

const uint8_t NUM_STEP_LENGTHS(13);

struct TransportParams
{
  TransportParams();
  ~TransportParams() = default;

  bool        wasReset_;
  bool        inReverse_;
  bool        anchorBit0_;
  bool        resetPending_;
  bool        newLoadPending_;
  bool        newPatternLoaded_;
  bool        load;

  ShiftParams shifter_;

  int8_t      offset_;
  int8_t      drunkStep_;
  uint8_t     nextPattern_;
  uint8_t     currentBankIdx_;

  std::array<const uint8_t, NUM_STEP_LENGTHS>::iterator workingLength;

  bool        wasNewPatternLoaded() const;
  bool        wasReset() const;
  int8_t      getStep() const;
  uint8_t     getSlot() const;
  uint8_t     getLength() const;

  void        reAnchor();
  void        lengthPLUS();
  void        lengthMINUS();
  void        setNextPattern(uint8_t slot);

  void        pre_iterate(int8_t steps, bool inPlace);
  uint16_t    loadPattern(const std::array<uint16_t, 8> &bank);
  uint16_t    iterate(uint16_t reg, Stochasticizer stoch);
  uint16_t    rotateToZero(const uint16_t reg);
};

#endif