// ------------------------------------------------------------------------
// CtrlPanel.h
//
// Aug. 2024
// Ryan "Ratimus" Richardson
// ------------------------------------------------------------------------
#ifndef GATE_IN_H
#define GATE_IN_H
#include "Arduino.h"
#include "DirectIO.h"


class GateInABC
{
protected:

  const uint8_t NUM_GATES;
  static const uint8_t MAX_GATES = 32;
  virtual uint32_t readPins() = 0;
  volatile uint32_t _gates;
  volatile uint32_t _gatesDiff;
  volatile uint32_t _rising;
  volatile uint32_t _falling;
  GateInABC(const uint8_t numGates):
  NUM_GATES(numGates)
  {
    assert(numGates < 33);
    reset();
  }

public:

  void reset()
  {
    cli();
    _gates = 0;
    _gatesDiff = 0;
    _rising = 0;
    _falling = 0;
    sei();
  }

  virtual void service()
  {
    cli();
    uint32_t prev(_gates);
    _gates = readPins();
    _gatesDiff = _gates ^ prev;
    sei();
  }

  bool readRiseFlag(uint8_t gate)
  {
    cli();
    bool ret(bitRead(_rising, gate));
    bitWrite(_rising, gate, 0);
    sei();
    return ret;
  }

  bool readFallFlag(uint8_t gate)
  {
    cli();
    bool ret(bitRead(_falling, gate));
    bitWrite(_falling, gate, 0);
    sei();
    return ret;
  }

  virtual bool peekGate(uint8_t gate) = 0;
  virtual bool peekDiff(uint8_t gate) = 0;
};


class GateInArduino : public GateInABC
{
protected:
  uint8_t INPUT_MAP[MAX_GATES];
  bool _pullup;

  virtual uint32_t readPins() override
  {
    uint32_t ret(0);

    for (uint8_t gate(0); gate < NUM_GATES; ++gate)
    {
      bool val(digitalRead(INPUT_MAP[gate]) ^ _pullup);
      bitWrite(ret, gate, val);
    }
    return ret;
  }

public:
  GateInArduino(const uint8_t numGates, const uint8_t pins[], bool pullup = false) :
    GateInABC(numGates),
    _pullup(pullup)
  {
    for (auto gateNum(0); gateNum < NUM_GATES; ++gateNum)
    {
      uint8_t pinNum(pins[gateNum]);
      INPUT_MAP[gateNum] = pinNum;
      pinMode(pinNum, INPUT);
      if (_pullup)
      {
        digitalWrite(pinNum, HIGH);
      }
    }
  }


  virtual bool peekGate(uint8_t gate) override
  {
    bool ret;
    cli();
    ret = _gates & (1 << gate);
    sei();
    return ret;
  }

  virtual bool peekDiff(uint8_t gate) override
  {
    bool ret;
    cli();
    ret = _gatesDiff & (1 << gate);
    sei();
    return ret;
  }
};


class GateInEsp32 : public GateInArduino
{
public:
  GateInEsp32(const uint8_t numGates, const uint8_t pins[], bool pullup = false) :
    GateInArduino(numGates, pins, pullup)
  {
    ;
  }

  virtual uint32_t readPins() override
  {
    uint32_t ret(0);
    for (uint8_t gate(0); gate < NUM_GATES; ++gate)
    {
      bool val(directRead(INPUT_MAP[gate]) ^ _pullup);
      bitWrite(ret, gate, val);
    }
    return ret;
  }
};

#endif