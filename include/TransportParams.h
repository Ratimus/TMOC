#ifndef TRANSPORT_PARAMS_DOT_H
#define TRANSPORT_PARAMS_DOT_H
#include <Arduino.h>
#include "stoch.h"
#include "ShiftParams.h"

const uint8_t NUM_STEP_LENGTHS(13);

class TransportParams
{
  bool        wasReset_;
  bool        readyToLoad_;
  bool        resetPending_;
  bool        newLoadPending_;
  bool        newPatternLoaded_;

  ShiftParams shifter_;

  int8_t      offset_;
  uint8_t     nextPattern_;
  uint8_t     currentBankIdx_;

  std::array<const uint8_t, NUM_STEP_LENGTHS>::iterator workingLength;

public:

  TransportParams();
  ~TransportParams() = default;

  bool        wasReset() const;
  bool        readyToLoad() const;
  bool        resetPending() const;
  bool        newLoadPending() const;
  bool        newPatternLoaded() const;

  int8_t      getStep() const;
  uint8_t     getSlot() const;
  uint8_t     getLength() const;
  uint8_t     currentBankIdx() const;

  void        reset();
  void        reAnchor();
  void        lengthPLUS();
  void        lengthMINUS();
  void        flagForReset();
  void        setNextPattern(const uint8_t slot);

  void        pre_iterate(const int8_t steps, const bool inPlace);
  uint16_t    loadPattern(const std::array<uint16_t, 8> &bank);
  uint16_t    iterate(uint16_t reg, Stochasticizer stoch);
  uint16_t    rotateToZero(const uint16_t reg);

  int8_t      drunkStep_;
};

#endif