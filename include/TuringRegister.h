#ifndef TURING_REGISTER_DOT_AITCH
#define TURING_REGISTER_DOT_AITCH

#include <Arduino.h>
#include <bitHelpers.h>
#include <MagicButton.h>
#include <ESP32AnalogRead.h>


struct Casino
{
  float THRESH_LOW;
  float THRESH_HIGH;

  ESP32AnalogRead *pVolts;
  MagicButton *pToggleUp;
  MagicButton *pToggleDown;
  bool togglesActive;

  Casino(
    ESP32AnalogRead * voltage,
    MagicButton * wrHI,
    MagicButton * wrLO):
    pVolts(voltage),
    pToggleUp(wrHI),
    pToggleDown(wrLO),
    togglesActive(false)
  {
    THRESH_LOW = 265.0;
    THRESH_HIGH = 3125.0;
  }

  bool coinToss(bool startVal)
  {
    if (togglesActive && ButtonState :: Held == pToggleUp->readAndFree())
    {
      return true;
    }

    if (togglesActive && ButtonState :: Held == pToggleDown->readAndFree())
    {
      return false;
    }

    float prob(pVolts->readMiliVolts());

    if (prob > THRESH_HIGH)
    {
      return startVal;
    }

    if (prob < THRESH_LOW)
    {
      return !startVal;
    }

    float result(float(random(0, 3135)));
    if (result > prob)
    {
      return !startVal;
    }

    return startVal;
  }
};


class TuringRegister
{
public:
  TuringRegister(
    ESP32AnalogRead * voltage,
    MagicButton * wrHI,
    MagicButton * wrLO):
    length(8),
    programCounter(0),
    startingStep(0),
    offset(0),
    shiftReg(0),
    casino(Casino(voltage, wrHI, wrLO))
  {
    ;
  }

  void setLength(uint8_t steps)
  {
    if (steps < 2 || steps > 32)
    {
      return;
    }
    length = steps;
  }

  void iterate(int8_t steps = -1)
  {
    uint8_t lastBitIndex(length - 1);
    uint8_t readIndex;
    uint8_t writeIndex;
    uint8_t leftAmt;
    uint8_t rightAmt;
    bool nextBit;
    if (steps > 0)  // Advance pattern
    {
      readIndex = lastBitIndex;
      writeIndex = 0;
      leftAmt = 1;
      rightAmt = 0;
    }
    else
    {
      steps = -steps;
      readIndex = 0;
      writeIndex = lastBitIndex;
      leftAmt = 0;
      rightAmt = 1;
    }

    for (uint8_t st(0); st < steps; ++st)
    {
      nextBit = bitRead(shiftReg, readIndex);
      nextBit = casino.coinToss(nextBit);
      if (leftAmt)
      {
        shiftReg = shiftReg << leftAmt;
      }
      else
      {
        shiftReg = shiftReg >> rightAmt;
      }
      bitWrite(shiftReg, writeIndex, nextBit);
    }
  }

  uint8_t getOutput()
  {
    return pattern;
  }

  void jam(uint16_t jamVal)
  {
    shiftReg = jamVal;
  }


protected:
  uint8_t length;
  uint8_t programCounter;
  uint8_t startingStep;
  uint8_t offset;
  union
  {
    uint8_t pattern;
    uint16_t shiftReg;
  };
  Casino casino;
};


#endif
