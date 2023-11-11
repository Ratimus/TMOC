#include <FastShiftOut.h>

// Analog ins
const uint8_t LOOP_CTRL(A0);
const uint8_t LOOP_CV(A1);
const uint8_t LENGTH_CTRL(A2);

// Digital ins
const uint8_t WRITE_HI(2);
const uint8_t WRITE_LO(3);
const uint8_t CLOCK_IN(4);

// 74HC595 Control
const uint8_t SR_CK(5);
const uint8_t SR_LT(6);
const uint8_t SR_DS(7);

FastShiftOut FSO(SR_DS, SR_CK, LSBFIRST);

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

void printPattern(byte pattBuf)
{
  digitalWrite(SR_LT, LOW);
  FSO.write(pattBuf & 255);
  digitalWrite(SR_LT, HIGH);
}

bool useInternalClock(false);

#define RLR_DEBUG
void setup()
{
  // Analog ins
  pinMode(LOOP_CTRL, INPUT);
  pinMode(LOOP_CV, INPUT);
  pinMode(LENGTH_CTRL, INPUT);
  
  // Digital ins
  pinMode(WRITE_HI, INPUT_PULLUP);
  pinMode(WRITE_LO, INPUT_PULLUP);
  
#ifdef RLR_DEBUG
  pinMode(CLOCK_IN, INPUT_PULLUP);
#else
  pinMode(CLOCK_IN, INPUT);
#endif

  // 74HC595 Control
  pinMode(SR_CK, OUTPUT);
  pinMode(SR_DS, OUTPUT);
  pinMode(SR_LT, OUTPUT);

  randomSeed(analogRead(5));
  patternBuf = random(0xffff);
  printPattern(patternBuf);
  
#ifdef RLR_DEBUG
  printPattern(0);
  Serial.begin(9600);
  delay(400);
  while (!Serial)
  {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  byte myByte = 0;
  
  useInternalClock = true;//!digitalRead(CLOCK_IN);
  while (digitalRead(CLOCK_IN))
  {
    printPattern(myByte);
    delay(50);
    if (!myByte) ++myByte;
      else myByte = myByte << 1;
    myByte %= 256;
    /*
    // if we get a valid byte, read analog ins:
    if (Serial.available() > 0)
    {
      byte newByte(Serial.read());
      if (newByte == 10)
      {
        Serial.println("");
        Serial.println("ENTER");
        Serial.println("");
        printPattern(myByte);
        myByte = 0;
        continue;
      }

      if ((newByte < 48 || newByte > 57) && newByte != 10)
      {
        Serial.print((char)newByte);
        continue;
      }

      newByte -= 48;
      Serial.print(newByte);
      myByte *= 10;
      myByte += newByte;
    }*/
  }
#endif
}

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
      
      bool flipBit(false);
      
      // TODO: weight LOOP CTRL and also read LOOP CV
      // NOTE: might want caps on WRITE SWITCH so we don't bounce      
      if (!digitalRead(WRITE_HI))
      {
        thisBit = true;
        Serial.println("WRITE_HI = 0");
      }
      else if (!digitalRead(WRITE_LO))
      {
        thisBit = false;
        Serial.println("WRITE_LO = 0");
      }
      else
      {
        thisBit = bitRead(patternBuf, endOfLoop);
        
        loopCtrlVal = analogRead(LOOP_CTRL);
        cvInVal = analogRead(LOOP_CV);
//RLR_DEBUG
cvInVal = 0;

        loopLottery = loopCtrlVal + cvInVal;
        loopLottery = min(loopLottery, TEN_BIT_MAX);

        if (loopLottery >= NEVER_FLIP)
        {
          // Have a nice day
        }
        else if (loopLottery <= ALWAYS_FLIP)
        {
          thisBit = !thisBit;
        }
        else
        {
          rando = random(ALWAYS_FLIP, NEVER_FLIP);

          if (rando > loopLottery)
          {
            thisBit = !thisBit;
            Serial.println("                 flip ^ ");
            Serial.println(" v here");
          }
        }
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
