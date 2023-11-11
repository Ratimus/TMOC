#include <FastShiftOut.h>


const int PATTERN_LEN [] = {2,3,4,5,6,8,12,16};
const int NUM_LENGTHS = sizeof(PATTERN_LEN) / sizeof(int);
const int MAX_STEPS(16);

const int TEN_BIT_MAX(1023);
const int LOOP_MIN(0);
const int LOOP_MAX(TEN_BIT_MAX);

const int ANALOG_THRESH(30); // Defines absolute range of loop ctrl; past either end removes randomness
const int ALWAYS_FLIP(LOOP_MIN + ANALOG_THRESH);
const int NEVER_FLIP(LOOP_MAX - ANALOG_THRESH);

// State variables

int loopCtrlVal(LOOP_MAX);
int lengthCtrlVal(0);
int cvInVal(0);

int loopLottery(0);
int rando(0);

bool clockState(false);
int pattLen(MAX_STEPS);
int endOfLoop(pattLen - 1);

bool thisBit(false);
uint16_t patternBuf(0x0000);
uint16_t lastBuf(0x0000);
uint16_t nextBuf(0x0000);


void loop()
{
  // RLR_DEBUG: enabled INPUT_PULLUP on CLOCK input for breadboard testing
  static int lastLen(-1);
  static int loopCount(0);
#ifdef RLR_DEBUG
  static bool lastClock(1);
  static bool thisClock(1);

  if (useInternalClock)
  {
    thisClock = (millis()/160) % 2;
  }
  else
  {
    thisClock = !digitalRead(CLOCK_IN);
    if (thisClock)
    {
      delay(15);
    }
  }
#else
  static bool lastClock(0);
  static bool thisClock(0);
  thisClock = digitalRead(CLOCK_IN);
#endif

  if (thisClock != lastClock)
  {
    lastClock = thisClock;

    if (thisClock)
    {
      // Work your Turing Magic
      // TODO: add hysteresis to lengthCtrl if necessary
      lengthCtrlVal = analogRead(LENGTH_CTRL);
      int lenIdx = map(lengthCtrlVal, 0, TEN_BIT_MAX, 0, NUM_LENGTHS - 1);

      pattLen = PATTERN_LEN [lenIdx];
      if (pattLen != lastLen)
      {
        if (pattLen > lastLen)
        {

        }
        else
        {
          nextBuf = patternBuf;
        }

        endOfLoop = pattLen - 1;
        lastLen = pattLen;
        Serial.print("Pattern Length: ");
        Serial.print(pattLen);
        Serial.println("");
      }

      

      // RFI: allow for non-zero start index to offset patterns
      // NB: LSB is BIT 0 (rightmost bit)
      patternBuf = patternBuf << 1;
      patternBuf = patternBuf & 0B1111111111111111;
      bitWrite(patternBuf, 0, thisBit);
      printPattern(patternBuf);

#ifdef RLR_DEBUG
      for (int ii = 0; ii < 8; ++ii)
      {
        Serial.write(bitRead(patternBuf, ii) & 1 == 1 ? " o " : "[ ]"); // will reverse bit order!
      }
      Serial.println("");
#endif
    }
    else // Falling edge of clock
    {
      if (loopCount == endOfLoop)
      {
        Serial.print("----Seq. length: ");
        Serial.print(pattLen);
        Serial.println(" --------------------");
      }
      ++loopCount;
      loopCount %= pattLen;
    }
  }
} // END OF VOID LOOP()
