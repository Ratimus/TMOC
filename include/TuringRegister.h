// ------------------------------------------------------------------------
// TuringRegister.h
//
// December 2023
// Ryan "Ratimus" Richardson
// ------------------------------------------------------------------------
#ifndef TURING_REGISTER_DOT_AITCH
#define TURING_REGISTER_DOT_AITCH

#include <Arduino.h>
#include <bitHelpers.h>
#include <MagicButton.h>
#include <vector>
#include "stoch.h"
#include "hw_constants.h"


const uint8_t NUM_STEP_LENGTHS(13);
const uint8_t STEP_LENGTH_VALS[NUM_STEP_LENGTHS]{2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 16};


class TuringRegister
{
public:
  TuringRegister(Stochasticizer& stoch);

  ~TuringRegister();

  uint8_t pulseIt();

  // Returns a register with the first [len] bits of [reg] copied into it
  // enough times to fill it to the end
  uint16_t norm(uint16_t reg, uint8_t len) const;

  // Shifts register
  void iterate(int8_t steps, bool inPlace = false);

  // Rotates the working register back to step 0
  void returnToZero();
  void reset();
  void changeLen(int8_t amt);

  void setBit()                           { stoch_.bitSetPending_ = true;   stoch_.bitClearPending_ = false;}
  void clearBit()                         { stoch_.bitClearPending_ = true; stoch_.bitSetPending_ = false;  }
  // Returns the current base pattern (i.e. the stored one, not the working one)
  uint16_t getPattern() const             { return *pShiftReg; }

  // Returns the working buffer
  uint8_t getOutput() const               { return (uint8_t)(workingRegister & 0xFF); }
  uint8_t getLength() const               { return *pPatternLength; }
  int8_t  getStep() const                 { return offset_; }
  uint8_t getSlot() const                 { return currentBankIdx_; }

  uint16_t getReg(uint8_t slot) const     { return patternBank[slot]; }
  uint8_t  getLength(uint8_t slot) const  { return lengthsBank[slot]; }

  void setNextPattern(uint8_t loadSlot);
  bool newPattern()                       { return newPatternLoaded_;}
  bool wasReset()                         { return wasReset_; }

  void writeToRegister(uint16_t fillVal, uint8_t bankNum);
  void savePattern(uint8_t bankIdx);
  void reAnchor() { offset_ = 0; }

  uint8_t getDrunkenIndex();
protected:
  void loadPattern();

  Stochasticizer        stoch_;

  bool                  inReverse_;
  bool                  anchorBit0_;
  bool                  resetPending_;
  bool                  wasReset_;
  bool                  newPatternLoaded_;

  uint16_t              workingRegister;
  int8_t                offset_;
  uint8_t               stepCountIdx_;

  const uint16_t*       pShiftReg;
  const uint8_t*        pPatternLength;

  std::array<uint8_t,  NUM_BANKS>  lengthsBank;
  std::array<uint16_t, NUM_BANKS> patternBank;

  const uint8_t         NUM_PATTERNS;
  uint8_t               currentBankIdx_;
  uint8_t               nextPattern_;
  bool                  newLoadPending_;
  int8_t                drunkStep_;
};


#endif
