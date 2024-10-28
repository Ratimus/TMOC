// ------------------------------------------------------------------------
// TuringRegister.h
//
// December 2023
// Ryan "Ratimus" Richardson
// ------------------------------------------------------------------------
#ifndef TURING_REGISTER_DOT_AITCH
#define TURING_REGISTER_DOT_AITCH

#include <Arduino.h>
#include <memory>
#include <bitHelpers.h>
#include <MagicButton.h>
#include <vector>
#include "stoch.h"
#include "hw_constants.h"
#include "ShiftParams.h"
#include "TransportParams.h"


class TuringRegister
{
public:
  TuringRegister(Stochasticizer& stoch);
  ~TuringRegister() = default;

  uint8_t   pulseIt();

  // Returns a register with the first [len] bits of [reg] copied into it
  // enough times to fill it to the end
  uint16_t  norm(const uint16_t reg, const uint8_t len) const;

  // Shifts register
  void      iterate(int8_t steps, bool inPlace = false);

  // Rotates the working register back to step 0
  void      rotateToZero();
  void      reset();
  void      changeLen(int8_t amt);

  void      setNextPattern(uint8_t loadSlot);
  void      savePattern(uint8_t bankIdx);

  uint8_t   getDrunkenIndex();

  void      setBit();
  void      clearBit();

  uint16_t  getPattern() const;
  uint16_t  getReg(uint8_t slot) const;
  uint8_t   getOutput() const;
  uint8_t   getLength() const;

protected:

  const uint8_t   NUM_PATTERNS;

  std::array<uint16_t, NUM_BANKS>  registersBank;

  Stochasticizer  stoch_;
  ShiftParams     shifter_;
  TransportParams transport_;
  uint16_t        workingRegister;
};


#endif
