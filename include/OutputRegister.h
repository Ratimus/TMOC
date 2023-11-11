#ifndef OUTPUT_REGISTER_DOT_AITCH
#define OUTPUT_REGISTER_DOT_AITCH
#include <Arduino.h>
#include <Latchable.h>
#include <bitHelpers.h>
#include <FastShiftOut.h>


// enable
// set
// clock
// clockIn
// clear
// reset
// pending
// preEnable

template <typename T>
  class OutputRegister : public latchable<T>
{
public:
  OutputRegister(uint8_t clkPin, uint8_t dataPin, uint8_t csPin, const uint8_t mapping[]):
  latchable<T>(T(0)),
  CLK(clkPin),
  DAT(dataPin),
  LCH(csPin),
  MAP(mapping),
  BYTE_COUNT(sizeof(T)),
  NUM_BITS(sizeof(T) * 8)
  {
    SR = new FastShiftOut(DAT, CLK, LSBFIRST);
  }

  ~OutputRegister()
  {
    delete SR;
    SR = NULL;
    MAP = NULL;
  }

  T clock() override
  {
    latchable<T> :: clock();
    // Serial.printf("VAL = %d\n", latchable<T> :: Q);
    writeOutputRegister();
    // Serial.printf("REGISTER = %d\n", REGISTER);
    return Q();
  }

  void setReg(uint8_t val, uint8_t bytenum = 0)
  {
    T setVal(T(val) << (8 * bytenum));
    // Serial.printf("set reg %d to %d\n", bytenum, val);
    // Serial.print("before: 0b");
    // for (auto bb(0); bb < NUM_BITS; ++bb)
    // {
      // Serial.print(bitRead(D(), (NUM_BITS - 1 - bb)) ? "1" : "0");
    // }
    // Serial.print("\n");
    T mask(T(0xFF) << (8 * bytenum));
    T temp(D() & ~mask);
    temp |= setVal;
    latchable<T> :: set(temp);
    // Serial.print(" after: 0b");
    // for (auto bb(0); bb < NUM_BITS; ++bb)
    // {
      // Serial.print(bitRead(D(), (NUM_BITS - 1 - bb)) ? "1" : "0");
    // }
    // Serial.print("\n");
  }

  void rotateRight(int8_t amt = 1, uint8_t bytenum = 0)
  {
    // uint8_t tempByte((uint8_t)(temp >> (8 * bytenum)));
    uint8_t temp((D() >> (8 * bytenum)) & 0xFF);
    if (amt < 0)
    {
      uint8_t absAmt(-amt % 8);
      temp = ((uint16_t(temp) << absAmt) & 0xFF) | (temp >> (8 - absAmt));
      // Serial.printf(" pre rotate register %d left %d bits: 0b", bytenum, absAmt);
    }
    else
    {
      amt %= 8;
      if (amt > 0)
      {
        // Serial.printf("pre rotate register %d right %d bits: 0b", bytenum, amt);
        temp = (temp >> amt) | ((uint16_t(temp) << (8 - amt)) & 0xFF);
      }
    }
    T tempT(D());
    tempT &= ~(T(0xFF) << (8 * bytenum));
    tempT |= T(temp) << (8 * bytenum);
    // for (auto bb(0); bb < NUM_BITS; ++bb)
    // {
      // Serial.print(bitRead(D(), (NUM_BITS - 1 - bb)) ? "1" : "0");
    // }
    // Serial.print("\n");
    latchable<T> :: set(tempT);
    // Serial.print("                       post rotate: 0b");
    // for (auto bb(0); bb < NUM_BITS; ++bb)
    // {
      // Serial.print(bitRead(D(), (NUM_BITS - 1 - bb)) ? "1" : "0");
    // }
    // Serial.print("\n");
  }

  T Q()
  {
    return latchable<T> :: Q;
  }

  T D()
  {
    return latchable<T> :: D;
  }

protected:
  const uint8_t CLK;
  const uint8_t DAT;
  const uint8_t LCH;
  const uint8_t *MAP;
  const uint8_t NUM_BITS;
  const size_t BYTE_COUNT;
  FastShiftOut *SR;

  void writeOutputRegister(void)
  {
    memset(&REGISTER, 0, BYTE_COUNT);
    for (auto bitnum(0); bitnum < NUM_BITS; ++bitnum)
    {
      bitWrite(REGISTER, MAP[bitnum], bitRead(Q(), bitnum));
    }

    digitalWrite(LCH, LOW);
    for (uint8_t bytenum(0); bytenum < BYTE_COUNT; ++bytenum)
    {
      SR->write((uint8_t)(REGISTER >> (8 * bytenum)));
    }
    digitalWrite(LCH, HIGH);
  }

  T REGISTER;
};

#endif
