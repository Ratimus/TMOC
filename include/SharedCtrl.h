#ifndef SHARED_CONTROL_H
#define SHARED_CONTROL_H

#include <Arduino.h>
#include "MCP_ADC.h"
#include "Latchable.h"

// Unlock knob when within this percent difference from the lock value
static const uint8_t MAX_BUFFER_SIZE(128);
static const double  DEFAULT_THRESHOLD(0.05);

////////////////////////////////////////////////
//
class HardwareCtrl
{
protected:
  MCP3208*  pADC;
  const    uint8_t  ch;
  const    int16_t  adcMax;

  uint8_t  buffSize;
  volatile int16_t  buff[MAX_BUFFER_SIZE];
  volatile uint8_t  sampleIdx;
  int64_t  sum;
  volatile bool bufferReady;

public:

////////////////////////////////////////////////
//
 explicit HardwareCtrl(MCP3208* inAdc, uint8_t inCh,
                       uint8_t numSamps = 1) :
    pADC(inAdc),
    ch(inCh),
    adcMax(pADC->maxValue()),
    buffSize(constrain(numSamps, 1, MAX_BUFFER_SIZE-1)),
    sampleIdx(0),
    sum(0),
    bufferReady(false)
  {
    cli();
    for (uint8_t ii = 0; ii < MAX_BUFFER_SIZE; ++ii)
    {
      buff[ii] = pADC->analogRead(ch);
    }
    sampleIdx = buffSize;
    sei();
  }

  ////////////////////////////////////////////////
  //
  void service()
  {
    if (pADC == NULL)
    {
      return;
    }

    cli();
    buff[sampleIdx] = pADC->analogRead(ch);
    ++sampleIdx;
    if (sampleIdx >= buffSize)
    {
      sampleIdx   = 0;
      bufferReady = true;
    }
    sei();
  }

  ////////////////////////////////////////////////
  //
  bool isReady()
  {
    bool retVal;
    cli();
    retVal = bufferReady;
    sei();
    return retVal;
  }

  ////////////////////////////////////////////////
  //
  // Get the value of the current reading
  virtual int16_t read()
  {
    uint8_t samps;
    int16_t retVal;
    sum = 0;

    cli();
    if (!bufferReady)
    {
      samps  = 0;
      retVal = buff[0];
    }
    else
    {
      samps = buffSize;
    }
    sei();

    if (samps == 0)
    {
      return retVal;
    }

    for (uint8_t ii = 0; ii < buffSize; ++ii)
    {
      cli();
      sum += buff[ii];
      sei();
    }

    return (sum / (int16_t)buffSize);
  }

  ////////////////////////////////////////////////
  //
  virtual int16_t maxValue()
  {
    return adcMax;
  }
};


////////////////////////////////////////////////
//
enum LockState
{
  STATE_UNLOCKED = 0,
  STATE_UNLOCK_REQUESTED,
  STATE_LOCKED
};


////////////////////////////////////////////////
//
class LockingCtrl
{
  friend class ModalCtrl;

protected:
  HardwareCtrl*      pCTRL;
  const int16_t      adcMax;
  uint16_t           threshInt;
  volatile LockState state;

private:
  int16_t lockVal;

public:

  ////////////////////////////////////////////////
  //
  friend bool operator== (const LockingCtrl& L1, const LockingCtrl& L2)
  {
    // Serial.printf("L1: %p;  L2: %p\n", &L1, &L2);
    return (&L1 == &L2);
  }

  ////////////////////////////////////////////////
  //
  friend bool operator!= (const LockingCtrl& L1, const LockingCtrl& L2)
  {
    // Serial.printf("L1: %p;  L2: %p\n", &L1, &L2);
    return (&L1 != &L2);
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////
  // (        (       (      (     (    (   (  ( (KONSTRUKTOR) )  )   )    )     )      )       )        ) //
  explicit LockingCtrl(
    HardwareCtrl* inCtrl,
    int16_t        inVal = 0,
    bool    createLocked = true):
  pCTRL   (inCtrl),
  adcMax  (pCTRL->maxValue()),
  lockVal (inVal)
  {
    threshInt = static_cast<uint16_t>( (uint16_t)(DEFAULT_THRESHOLD * adcMax + 0.5) );

    if (createLocked)
    {
      jam(lockVal);
    }
  }


  ////////////////////////////////////////////////
  //
  int16_t maxValue()
  {
    return adcMax;
  }

  ////////////////////////////////////////////////
  //
  int16_t getLockVal()
  {
    int16_t retVal;
    retVal = lockVal;
    return retVal;
  }

  ////////////////////////////////////////////////
  //
  LockState getLockState()
  {
    LockState tmpState;
    cli();
    tmpState = state;
    sei();
    return tmpState;
  }

protected:

  ////////////////////////////////////////////////
  //
  virtual int16_t sample()
  {
    // Check current state
    LockState tmpState;
    cli();
    tmpState = state;
    sei();

    // Return lockVal if locked
    if (tmpState == STATE_LOCKED || !pCTRL->isReady())
    {
      int16_t retVal;
      retVal = lockVal;
      return retVal;
    }

    int16_t tmpVal(pCTRL->read());

    if (tmpState == STATE_UNLOCKED)
    {
      return tmpVal;
    }

    // STATE_UNLOCK_REQUESTED
    if (abs(lockVal - tmpVal) < threshInt)
    {
      cli();
      state = STATE_UNLOCKED;
      sei();
      Serial.printf("%p UNLOCKED @ %d [%d]\n", this, tmpVal, pCTRL->read());
      return tmpVal;
    }

    return lockVal;
  }

  ////////////////////////////////////////////////
  //
  void jam(int16_t jamVal)
  {
    bool lazy;
    LockState curState;
    cli();
    curState  = state;
    sei();
    lazy = (lockVal == jamVal && curState == STATE_LOCKED);

    if (lazy)
    {
      // Serial.printf("    %p LAZY LOCKED @ %d [%d]\n", this, jamVal, pCTRL->read());
      return;
    }

    lockVal = jamVal;
    cli();
    state   = STATE_LOCKED;
    sei();

    // Serial.printf("    %p was %s, now LOCKED @ %d [%d]\n", this, (curState == STATE_UNLOCKED) ? "UNLOCKED" : "REQ_UNLOCK", jamVal, pCTRL->read());
  }

  ////////////////////////////////////////////////
  //
  bool lock()
  {
    LockState tmpState;
    cli();
    tmpState = state;
    sei();

    if (tmpState == STATE_LOCKED)
    {
      return false;
    }

    cli();
    state = STATE_LOCKED;
    sei();

    jam(sample());

    return true;
  }

  ////////////////////////////////////////////////
  //
  void reqUnlock()
  {
    LockState tmpState;
    cli();
    tmpState = state;
    sei();

    if (tmpState != STATE_LOCKED) { return; }

    cli();
    state = STATE_UNLOCK_REQUESTED;
    sei();
  }
};

////////////////////////////////////////////////////////////////////////
// VIRTUAL CTRL
////////////////////////////////////////////////////////////////////////
class VirtualCtrl : public LockingCtrl
{
  const int16_t* loVal;
  const int16_t* hiVal;

  const bool pointsOut;

  int16_t lo;
  int16_t hi;

  int16_t lockSlice;

public:

  static int16_t * sharedZERO;

  ////////////////////////////////////////////////
  //
  explicit VirtualCtrl(HardwareCtrl* inCtrl,
              int16_t inSlice,
              int16_t inHI,
              int16_t inLO         = 0,
              bool    createLocked = true):
    LockingCtrl(inCtrl, inSlice, true),
  hi(inHI),
  lo(inLO),
  pointsOut(false),
  hiVal(&hi),
  loVal(&lo),
  lockSlice(inSlice)
  {
    if (createLocked)
    {
      jam(lockSlice);
    }
  }

  ////////////////////////////////////////////////
  //
  explicit VirtualCtrl(HardwareCtrl* inCtrl,
              int16_t  inSlice,
              int16_t* inHI,
              int16_t  hiOFFSET     = -1,
              int16_t* inLO         = sharedZERO,
              int16_t  loOFFSET     = 0,
              bool     createLocked = true):
    LockingCtrl(inCtrl, inSlice, true),
  hi(hiOFFSET),
  lo(loOFFSET),
  pointsOut(true),
  hiVal(inHI),
  loVal(inLO),
  lockSlice(inSlice)
  {
    if (createLocked)
    {
      jam(lockSlice);
    }
  }

  ////////////////////////////////////////////////
  //
  void updateRange(int16_t inHI, int16_t inLO = 0)
  {
    hi = inHI;
    lo = inLO;
  }

  ////////////////////////////////////////////////
  //
  int16_t sliceToVal(int16_t tgtSlice)
  {
    int16_t rangeHi(*hiVal + (int16_t)pointsOut * hi);
    int16_t rangeLo(*loVal + (int16_t)pointsOut * lo);

    int16_t tgtVal(map(tgtSlice, rangeLo, rangeHi+1, 0, adcMax+1));
    return tgtVal;
  }

  ////////////////////////////////////////////////
  //
  int16_t valToSlice(int16_t val)
  {
    int16_t rangeHi(*hiVal + (int16_t)pointsOut * hi);
    int16_t rangeLo(*loVal + (int16_t)pointsOut * lo);

    return map(val, 0, adcMax+1, rangeLo, 1+rangeHi);
  }

  ////////////////////////////////////////////////
  //
  virtual int16_t sample() override
  {
    // Check current state
    LockState tmpState;
    cli();
    tmpState = state;
    sei();

    // Return lockVal if locked
    if (tmpState == STATE_LOCKED || !pCTRL->isReady())
    {
      return lockSlice;
    }


    int16_t rangeHi(*hiVal + (int16_t)pointsOut * hi);
    int16_t rangeLo(*loVal + (int16_t)pointsOut * lo);

    int16_t tmpVal(pCTRL->read());
    int16_t tmpSlice(valToSlice(tmpVal));

    if (tmpState == STATE_UNLOCK_REQUESTED)
    {
      if (tmpSlice == lockSlice)
      {
        cli();
        state = STATE_UNLOCKED;
        sei();
        // Serial.printf("%p LOCK->UNLOCK; slice: %d, lockSlice: %d, val: %d\n", this, tmpSlice,lockSlice, tmpVal);
        // Serial.printf(" hiVal: %d, loVal: %d, rangeHi: %d, rangeLo: %d\n",*hiVal, *loVal, rangeHi, rangeLo);
      }
    }
    else if(tmpSlice > lockSlice)
    {
      int16_t exc(tmpVal - sliceToVal(tmpSlice));
      if (exc > 50)
      {
        lockSlice = tmpSlice;
        // Serial.printf("%p @ slice %d [%d] (tgt val: %d)\n", this, tmpSlice, tmpVal, sliceToVal(tmpSlice));
      }
    }
    else
    {
      int16_t exc(sliceToVal(lockSlice) - tmpVal);
      if (exc > 50)
      {
        // Serial.printf("%p @ slice %d [%d] (tgt val: %d)\n", this, tmpSlice, tmpVal, sliceToVal(lockSlice));
        lockSlice = tmpSlice;
      }
    }

    return lockSlice;
  }
};


////////////////////////////////////////////////
//
class ArrayCtrl : public VirtualCtrl
{
  int16_t* pARR;
  const size_t size;
  uint8_t iter;

public:

  ////////////////////////////////////////////////
  //
  ArrayCtrl(HardwareCtrl* inCtrl,
            int16_t*      arr, size_t count, int16_t stIdx = 0):
    VirtualCtrl(inCtrl, stIdx, (int16_t)(count-1)),
  pARR(arr),
  size(count),
  iter(stIdx)
  { ; }

  ////////////////////////////////////////////////
  //
  int16_t sample() override
  {
    uint8_t tmp((uint8_t)(VirtualCtrl::sample()));
    tmp = constrain(tmp, 0, (int16_t)size-1);
    if (tmp != iter)
    {
      // Serial.printf("%p was %d [idx=%d], now is %d [idx=%d]\n",this,pARR[iter],iter,pARR[tmp],tmp);
      iter = tmp;
    }
    return pARR[iter];
  }
};


////////////////////////////////////////////////
//
class ModalCtrl
{
  uint8_t numModes;
  uint8_t selMode;

public:
  VirtualCtrl** ctrlOBJECTS; // Array of ptrs (NOT a ptr to an array)
  VirtualCtrl*  pACTIVE;

  ////////////////////////////////////////////////
  //
  explicit ModalCtrl(uint8_t inNum, VirtualCtrl* knobArr[]) :
    numModes(inNum),
    selMode(0),
    ctrlOBJECTS(knobArr),
    pACTIVE(ctrlOBJECTS[0])
  { ; }

  ////////////////////////////////////////////////
  //
  uint8_t getNumModes()
  {
    return numModes;
  }

  ////////////////////////////////////////////////
  //
  int16_t readActiveCtrl()
  {
    return pACTIVE->sample();
  }

  ////////////////////////////////////////////////
  //
  uint8_t  getSelMode() { return selMode; }

  ////////////////////////////////////////////////
  //
  uint16_t maxValue()   { return pACTIVE->maxValue(); }

  ////////////////////////////////////////////////
  //
  uint8_t select(uint8_t sel)
  {
    selMode = sel % numModes;
    pACTIVE->lock();

    if (*(ctrlOBJECTS+sel) != pACTIVE)
    {
      pACTIVE = *(ctrlOBJECTS+selMode);
    }

    pACTIVE->reqUnlock();
    return selMode;
  }

  ////////////////////////////////////////////////
  //
  int16_t peek(uint8_t sel)
  {
    selMode = sel % numModes;
    return (*(ctrlOBJECTS[selMode])).getLockVal();
  }

  ////////////////////////////////////////////////
  //
  int16_t lock()
  {
    pACTIVE->lock();
    return pACTIVE->getLockVal();
  }

  ////////////////////////////////////////////////
  //
  void lockSel(uint8_t sel)
  {
    ctrlOBJECTS[sel]->lock();
  }

  ////////////////////////////////////////////////
  //
  void jam(int16_t jamVal)
  {
    pACTIVE->jam(jamVal);
  }
};

#endif