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
#include <ESP32AnalogRead.h>
#include <vector>


const uint8_t NUM_STEP_LENGTHS(13);
const uint8_t STEP_LENGTH_VALS[NUM_STEP_LENGTHS]{2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 16};


class TuringRegister
{
public:
  TuringRegister(
    ESP32AnalogRead *voltage,
    ESP32AnalogRead *cv);

  ~TuringRegister();

  // Returns a register with the first [len] bits of [reg] copied into it
  // enough times to fill it to the end
  uint16_t norm(uint16_t reg, uint8_t len) const;

  // Shifts register, returns current step [i.e. prior to advancing]
  uint8_t iterate(int8_t steps);

  // Rotates the working register back to step 0
  void reset();
  void rotateToCurrentStep();
  void lengthPLUS();
  void lengthMINUS();

  // Returns the current base pattern (i.e. the stored one, not the working one)
  uint16_t getPattern() const             { return *pShiftReg; }

  // Returns the working buffer
  uint8_t getOutput() const               { return (uint8_t)(workingRegister & 0xFF); }
  uint8_t getLength() const               { return *pPatternLength; }
  int8_t  getStep() const                 { return offset_; }

  uint16_t getReg(uint8_t slot) const     { return patternBank[slot]; }
  uint8_t  getLength(uint8_t slot) const  { return lengthsBank[slot]; }

  void writeBit(uint8_t idx, bool bitVal);
  void writeToRegister(uint16_t fillVal, uint8_t bankNum);
  void loadPattern(uint8_t bankIdx, bool saveFirst = false);
  void savePattern(uint8_t bankIdx);
  void reAnchor() { offset_ = 0; }

protected:

  struct Stochasticizer
  {
    float THRESH_LOW;
    float THRESH_HIGH;

    ESP32AnalogRead * const pVolts;
    ESP32AnalogRead * const pCV;

    Stochasticizer(
      ESP32AnalogRead * const voltage,
      ESP32AnalogRead * const cv);

    bool stochasticize(const bool startVal) const;
  };

  Stochasticizer        stoch_;

  bool                  inReverse_;
  bool                  anchorBit0_;
  bool                  resetPending_;

  uint16_t              workingRegister;
  int8_t                offset_;
  uint8_t               stepCountIdx_;

  const uint16_t*       pShiftReg;
  const uint8_t*        pPatternLength;
  std::vector<uint8_t>  lengthsBank;
  std::vector<uint16_t> patternBank;

  const uint8_t         NUM_PATTERNS;
  uint8_t               currentBankIdx_;
};


#endif
