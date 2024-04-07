// ------------------------------------------------------------------------
// SharedCtrl.h
//
// January 2024
// Ryan "Ratimus" Richardson
// ------------------------------------------------------------------------
#ifndef SHARED_CONTROL_H
#define SHARED_CONTROL_H

#include <Arduino.h>
#include "MCP_ADC.h"
#include "Latchable.h"

// Maximum number of samples to average (higher values smooth noise)
static const uint8_t MAX_BUFFER_SIZE(128);

// Unlock knob when within this percent difference from the lock value
static const double  DEFAULT_THRESHOLD(0.05);

////////////////////////////////////////////////
// Wraps an ADC channel for use with control classes
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
// Constructor
 explicit HardwareCtrl(
  MCP3208* inAdc,
  uint8_t inCh,
  uint8_t numSamps = 1):
    pADC       (inAdc),
    ch         (inCh),
    adcMax     (pADC->maxValue()),
    buffSize   (constrain(numSamps, 1, MAX_BUFFER_SIZE-1)),
    sampleIdx  (0),
    sum        (0),
    bufferReady(false)
  {
    cli();
    for (uint8_t ii = 0; ii < MAX_BUFFER_SIZE; ++ii)
    {
      // Fill buffer so we already have a good average to start with
      buff[ii] = pADC->analogRead(ch);
    }
    sampleIdx = buffSize;
    sei();
  }

  ////////////////////////////////////////////////
  // Call this in an ISR at like 1ms or something
  void service()
  {
    if (pADC == NULL)
    {
      return;
    }

    cli();
    // Add one sample to the buffer
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
  // Report 'ready' if buffer is full
  bool isReady()
  {
    bool retVal;
    cli();
    retVal = bufferReady;
    sei();
    return retVal;
  }

  ////////////////////////////////////////////////
  // Get the (smoothed) raw ADC value
  virtual int16_t read()
  {
    uint8_t samps;
    int16_t retVal;
    sum = 0;

    cli();
    // Buffer isn't full; just return the first sample
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

    // Take the mean of all samples in the buffer
    for (uint8_t ii = 0; ii < buffSize; ++ii)
    {
      cli();
      sum += buff[ii];
      sei();
    }

    return (sum / (int16_t)buffSize);
  }

  ////////////////////////////////////////////////
  // Get the highest value the ADC can return
  virtual int16_t maxValue()
  {
    return adcMax;
  }
};


////////////////////////////////////////////////
// UNLOCKED:         control value is whatever the current reading is
// UNLOCK_REQUESTED: control will unlock if/when current reading matches lock value,
//                   else it returns lock value
// LOCKED:           ignore vurrent reading and return lock value
enum LockState
{
  STATE_UNLOCKED = 0,
  STATE_UNLOCK_REQUESTED,
  STATE_LOCKED
};


////////////////////////////////////////////////
// Defines a control that can be locked and unlocked
// You probably won't instantiate one of these directly. Rather,
// you'll create a VirtualControl (which extends this class).
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
  // Double equal all the way across the skyh
  friend bool operator== (const LockingCtrl& L1, const LockingCtrl& L2)
  {
    // Serial.printf("L1: %p;  L2: %p\n", &L1, &L2);
    return (&L1 == &L2);
  }

  ////////////////////////////////////////////////
  // Some objects are more equal than others
  friend bool operator!= (const LockingCtrl& L1, const LockingCtrl& L2)
  {
    // Serial.printf("L1: %p;  L2: %p\n", &L1, &L2);
    return (&L1 != &L2);
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Constructor
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
  // Get the max value the ADC is capable of returning
  int16_t maxValue()
  {
    return adcMax;
  }

  ////////////////////////////////////////////////
  // Get the lock value regardles of lock state
  int16_t getLockVal()
  {
    int16_t retVal;
    retVal = lockVal;
    return retVal;
  }

  ////////////////////////////////////////////////
  // Can you guess what this does?
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
  // Returns current ADC reading if unlocked, else returns locked value
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
      // Serial.printf("%p UNLOCKED @ %d [%d]\n", this, tmpVal, pCTRL->read());
      return tmpVal;
    }

    return lockVal;
  }

  ////////////////////////////////////////////////
  // Ignore current reading, overwrite the lock value with jamVal, and lock the control
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
  // Lock the control at its current value if it isn't already locked
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
  // Activates the control; it can now be unlocked
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
// Inherits from LockingControl; returns a limited number of options rather
// than a raw ADC value and uses hysteresis to prevent erratic mode-switching
class VirtualCtrl : public LockingCtrl
{
  int16_t minSlice;
  int16_t maxSlice;
  int16_t lockSlice;

public:

  static int16_t * sharedZERO;

  int16_t getMin() { return minSlice; }
  int16_t getMax() { return maxSlice; }
  ////////////////////////////////////////////////
  //
  // Constructor
  explicit VirtualCtrl(
    HardwareCtrl* inCtrl,
    int16_t inSlice,
    int16_t inHI,
    int16_t inLO         = 0,
    bool    createLocked = true):
      LockingCtrl(
        inCtrl,
        inSlice,
        true),
      maxSlice(inHI),
      minSlice(inLO),
      lockSlice(inSlice)
  {
    if (createLocked)
    {
      jam(lockSlice);
    }
  }

  void jam (int16_t slice)
  {
    lockSlice = slice;
    LockingCtrl :: jam(sliceToVal(lockSlice));
  }

  ////////////////////////////////////////////////
  // Change the range of values the control can return
  void updateRange(int16_t inHI, int16_t inLO = 0)
  {
    LockState currentState();
    bool wasLocked(getLockState() == LockState :: STATE_LOCKED);
    lock();
    minSlice = inLO;
    maxSlice = inHI;
    if (wasLocked)
    {
      return;
    }

    reqUnlock();
    sample();
  }

  ////////////////////////////////////////////////
  // Figure out what ADC reading you'd need to match the given control value [tgtSlice]
  int16_t sliceToVal(int16_t tgtSlice)
  {
    return map(tgtSlice, minSlice, maxSlice + 1, 0, adcMax + 1);
  }

  ////////////////////////////////////////////////
  // Get the control value corresponding to a given ADC value [val]
  int16_t valToSlice(int16_t val)
  {
    return map(val, 0, adcMax + 1, minSlice, maxSlice + 1);
  }

  ////////////////////////////////////////////////
  // Here's how you actually get the control value during typical use
  virtual int16_t sample() override
  {
    // Check current state
    LockState tmpState;
    cli();
    tmpState = state;
    sei();

    // Return lockVal if locked
    if (tmpState == STATE_LOCKED)
    {
      // Serial.printf("    %p LOCKED @ %u\n", this, lockSlice);
      return lockSlice;
    }

    if (!pCTRL->isReady())
    {
      // Serial.printf("    %p ADC not ready! lock val = %u\n", this, lockSlice);
      return lockSlice;
    }

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
        // Serial.printf(" pMax: %d, loVal: %d, rangeHi: %d, rangeLo: %d\n",*pMax, *loVal, rangeHi, rangeLo);
      }
      else
      {
        ;
        // Serial.printf("read slice: %u, lock slice: %u\n", tmpSlice, lockSlice);
      }

      return lockSlice;
    }

    int16_t exc;
    if(tmpSlice > lockSlice)
    {
      exc = (tmpVal - sliceToVal(tmpSlice));
    }
    else
    {
      exc = (sliceToVal(tmpSlice) - tmpVal);
    }

    if (exc > 50)
    {
      lockSlice = tmpSlice;
        // Serial.printf("%p @ slice %d [%d] (tgt val: %d)\n", this, tmpSlice, tmpVal, sliceToVal(tmpSlice));
    }

    return lockSlice;
  }
};


////////////////////////////////////////////////
//
// Defines an array of Virtual Controls based on a single hardware control
class ArrayCtrl : public VirtualCtrl
{
  int16_t* pARR;
  const size_t size;
  uint8_t iter;

public:

  ////////////////////////////////////////////////
  //
  ArrayCtrl(
    HardwareCtrl* inCtrl,
    int16_t*      arr,
    size_t count,
    int16_t stIdx = 0):
      VirtualCtrl(
        inCtrl,
        stIdx,
        (int16_t)(count-1)),
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
// Manager class to serve as a single point of interaction for an array of virtual controls
// in which only one virtual control is active at a time
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
  // Locks the current bank and activates sel
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

  void overWrite()
  {
    int16_t tmpVal(pACTIVE->pCTRL->read());
    int16_t tmpSlice(pACTIVE->valToSlice(tmpVal));
    // Serial.printf("val: %u, slice: %u\n", tmpVal, tmpSlice);
    jam(tmpSlice);
    // Serial.println("    reqUnlock");
    // pACTIVE->reqUnlock();
    // Serial.printf("    lock val: %u\n", pACTIVE->getLockVal());
  }

  void setRange(uint8_t mode, int16_t max, int16_t min = 0)
  {
    ctrlOBJECTS[mode]->updateRange(max, min);
  }

  void copySettings(uint8_t dest, int8_t source = -1)
  {
    if (source < 0)
    {
      source = selMode;
    }
    ctrlOBJECTS[dest]->jam(ctrlOBJECTS[source]->sample());
    ctrlOBJECTS[dest]->updateRange(ctrlOBJECTS[source]->getMax(),  ctrlOBJECTS[source]->getMin());
    ctrlOBJECTS[source]->reqUnlock();
    ctrlOBJECTS[source]->sample();
  }
};

#endif