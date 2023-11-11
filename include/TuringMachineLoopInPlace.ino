#include <FastShiftOut.h>
// https://github.com/RobTillaart/FastShiftOut


#include <bitHelpers.h>
// https://github.com/RobTillaart/bitHelpers


// Analog ins
const uint8_t LOOP_CTRL(A0);
const uint8_t LOOP_CV(A1);
const uint8_t LENGTH_CTRL(A2);

// Digital ins
const uint8_t WRITE_HI(12);//2);
const uint8_t WRITE_LO(3);
const uint8_t CLOCK_IN(2);//4);

// 74HC595 Control
const uint8_t SR_CK(5);
const uint8_t SR_LT(6);
const uint8_t SR_DS(7);

FastShiftOut FSO(SR_DS, SR_CK, MSBFIRST);

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
int loopLottery(0);
int rando(0);

bool clockState(false);
uint8_t pattLen(MAX_STEPS);

bool thisBit(false);
uint16_t historyBuf(0x0000);
uint16_t patternBuf(0x0000);
uint16_t workingBuf(0x0000);

uint8_t playHead [] = {0,0,0};
uint8_t& SOL(playHead[0]);   // Start of loop
uint8_t& EOL(playHead[1]);   // End of loop
uint8_t& ROT_CT(playHead[2]); // Index in loop (use to sync history)

bool useInternalClock(false);
bool CLK [] = {0, 0};


/////////////////////////////////////////////////////
//
//  Forward Declarations of functions because Arduino
//   hates Object-Oriented programming so I haven't
//   made this OO yet (and may not ever)
//
int8_t setEOL(uint8_t thisEOL);
uint8_t updateHistory();
uint8_t endOfLoop();
uint8_t updateLoop();
void dumpRegister(uint16_t reg, bool LEDs = false);
void printPattern(byte pattBuf);
void clockUP();
void SerialGarbage(bool one);
//
//  END Forward Declarations of Function Prototypes
//
/////////////////////////////////////////////////////

#define RLR_DEBUG

/////////////////////////////////////////////////////
//
//  void setup()
//
/////////////////////////////////////////////////////
void setup()
{
  // Analog ins
  pinMode(LOOP_CTRL,    INPUT);
  pinMode(LOOP_CV,      INPUT);
  pinMode(LENGTH_CTRL,  INPUT);
  
  // Digital ins
  pinMode(WRITE_HI,     INPUT_PULLUP);
  pinMode(WRITE_LO,     INPUT_PULLUP);
  
#ifndef RLR_DEBUG
  pinMode(CLOCK_IN,     INPUT);
#else
  pinMode(CLOCK_IN,     INPUT_PULLUP);
#endif

  // 74HC595 Control
  pinMode(SR_CK, OUTPUT);
  pinMode(SR_DS, OUTPUT);
  pinMode(SR_LT, OUTPUT);

  randomSeed(analogRead(5));
  patternBuf = random(0xffff);
  
  // Retain snapshot
  historyBuf = patternBuf;
  printPattern(patternBuf);
  
#ifdef RLR_DEBUG
  printPattern(0);
  Serial.begin(9600);
  delay(400);
  while (!Serial) { ; }// wait for serial port to connect. Needed for native USB port only
  byte myByte = 0;
  
  useInternalClock = true;//!digitalRead(CLOCK_IN);
  while (digitalRead(CLOCK_IN))
  {
    printPattern(myByte);
    delay(50);
    
    if (!myByte) { ++myByte; }
    else { myByte = myByte << 1; }
    
    myByte %= 256;
  }
#endif
}


/////////////////////////////////////////////////////
//
//  void loop()
//
/////////////////////////////////////////////////////
void loop() 
{
  // RLR_DEBUG: enabled INPUT_PULLUP on CLOCK input for breadboard testing

#ifndef RLR_DEBUG
  CLK [1] = digitalRead(CLOCK_IN);
#else
  static bool firstLoop(true);
  if (firstLoop)
  {
    CLK [0] = 1;
    CLK [1] = 1;
    firstLoop = false;
  }
  useInternalClock=0;
  if (useInternalClock) { CLK [1] = (millis()/100) % 2; }
  else 
  { 
    CLK [1] = !digitalRead(CLOCK_IN);
    if (CLK [1]) { delay(15); }
  }
#endif // END STUPID DEBUG CLOCK HACK STUFF

  if (CLK [1] != CLK [0])
  {
    CLK [0] = CLK [1];

    if (CLK [1])
    {
      clockUP();
    }
    else // Falling edge of clock
    {
      // clockDown();
    }
  }
} // END OF VOID LOOP()


/////////////////////////////////////////////////////
//
//  void printPattern(byte pattBuf)
//
/////////////////////////////////////////////////////
void printPattern(byte pattBuf)
{
  digitalWrite(SR_LT, LOW);
  FSO.write(pattBuf & 255);
  digitalWrite(SR_LT, HIGH);
}


/////////////////////////////////////////////////////
//
//  void clockUP()
//
/////////////////////////////////////////////////////
void clockUP()
{
#ifndef RLR_DEBUG
  // The clean way:
  setEOL(endOfLoop());
  printPattern(updateLoop());
  
#else

  // The Dirty Debug:
  uint8_t newEOL(endOfLoop());
  setEOL(newEOL);
  uint8_t outBuf = updateLoop();
  printPattern(outBuf);

  dumpRegister(outBuf, true);
  Serial.print("        ");
  dumpRegister(outBuf);
#endif
}


/////////////////////////////////////////////////////
//
//  uint8_t updateHistory()
//
/////////////////////////////////////////////////////
uint8_t updateHistory()
{
  uint8_t bitsSaved(0);
  uint8_t subLoopPtr(ROT_CT % (EOL +1));
  
  historyBuf = bitRotateLeft(historyBuf, ROT_CT);
  ROT_CT = 0;

  // RLR_DEBUG
  SerialGarbage(1);

  // write bits 0:L-1 into history buffer
  for (uint8_t stepIdx = SOL; stepIdx <= EOL; ++stepIdx)
  {
    bitWrite(historyBuf, stepIdx, bitRead(patternBuf, stepIdx));
    ++bitsSaved;
  }

  SerialGarbage(0);
  return subLoopPtr;
}


/////////////////////////////////////////////////////
//
//  int8_t setEOL(uint8_t thisEOL)
//
/////////////////////////////////////////////////////
int8_t setEOL(uint8_t thisEOL)
{
  if (EOL == thisEOL) { return 0; }
  
  uint8_t lastEOL = EOL;
  uint8_t subIdx = updateHistory();
  Serial.print("prevous pattern was on step ");
  Serial.print(subIdx + 1);
  Serial.print(" of ");
  Serial.println(EOL + 1);
  if (thisEOL > lastEOL) { patternBuf = historyBuf; }
  
  EOL = thisEOL;
  return EOL - lastEOL;
}


/////////////////////////////////////////////////////
//
//  uint8_t endOfLoop()
//
/////////////////////////////////////////////////////
uint8_t endOfLoop()
{
  int lenIdx = map(
    analogRead(LENGTH_CTRL),
    0,
    TEN_BIT_MAX,
    0,
    NUM_LENGTHS - 1
  );
  
  return PATTERN_LEN [lenIdx] - 1;
}


/////////////////////////////////////////////////////
//
//  uint8_t updateLoop()
//
/////////////////////////////////////////////////////
uint8_t updateLoop()
{
  // Grap End of Loop bit
  bool eolBit(bitRead(patternBuf, EOL));

  patternBuf = bitRotateLeft(patternBuf, 1);

  // Keep track of how many times we've rotated left
  ++ROT_CT;
  ROT_CT %= MAX_STEPS;
  
  // TODO: weight LOOP CTRL and also read LOOP CV

  // RLR_DEBUG: here's where we could use other generative seeds:
  //  rather that CV = probability of flip, CV could be directly sampled to write 1 or 0.
  //  whether or not that CV *is* sampled could be controlled another way
  //  could directly sample chaos, where -V_min to -V_thresh is FALSE, -V_thresh to +V_thresh is THIRD STATE, +V_thresh to +V_max = TRUE
  //  could mix chaos and noise
  
  // Determine what to write in Bit 00
  if (!digitalRead(WRITE_HI))
  {
    eolBit = true;
    Serial.println("WRITE_HI = 0");
  }
  else if (!digitalRead(WRITE_LO))
  {
    eolBit = false;
    Serial.println("WRITE_LO = 0");
  }
  else
  {
    uint16_t loopVR(analogRead(LOOP_CTRL));
    uint16_t loopCV(analogRead(LOOP_CV));

    // RLR_DEBUG
    // Need HW scaling to safely read this without just clipping to 1 or zero
    loopCV = 0;

    uint16_t loopLottery(loopVR + loopCV);
    loopLottery = min(loopLottery, TEN_BIT_MAX);
    
    if (loopLottery >= NEVER_FLIP)
    {
      // Have a nice day
    }
    else if (loopLottery <= ALWAYS_FLIP)
    {
      eolBit = !eolBit;
    }
    else
    {
      rando = random(ALWAYS_FLIP, NEVER_FLIP);
      eolBit = (rando > loopLottery) ? !eolBit : eolBit;
    }
  }
  
  bitWrite(patternBuf, SOL, eolBit);
  return patternBuf & 255;
}


/////////////////////////////////////////////////////
//
//  void dumpRegister(uint16_t reg, bool LEDs = false)
//
/////////////////////////////////////////////////////
void dumpRegister(uint16_t reg, bool LEDs /*= false*/)
{
  uint16_t temp = 0xffff & reg;
  if (LEDs)
  {
    for (int ii = 0; ii < 8; ++ii)
    {
      bitRead(temp, ii) ? Serial.print(" o ") : Serial.print("[ ]");
    }
    return;
  }
  
  for (int ii = 15; ii >= 0; --ii)
  {
    bitRead(temp, ii) ? Serial.print(1) : Serial.print(0);
  }
  Serial.println(""); 
}


/////////////////////////////////////////////////////
//
//  void SerialGarbage(bool one)
//
/////////////////////////////////////////////////////
void SerialGarbage(bool one)
{
  if (one)
  {
    // Begin Serial.blahblahblah
    Serial.print("index: ");
    Serial.println(ROT_CT);
    Serial.print("Saved pattern:   ");
    dumpRegister(historyBuf);

    Serial.print("Current pattern: ");
    dumpRegister(patternBuf);
    Serial.print("                 ");
    int ii = 15;
    for ( ; ii > EOL; --ii)
    {
      Serial.print(" ");
    }
    for ( ; ii >= SOL; --ii)
    {
      Serial.print("|");
    }

    Serial.println("");
    Serial.print("                 ");
    ii = 15;
    for ( ; ii > EOL; --ii)
    {
      Serial.print(" ");
    }
    for ( ; ii >= SOL; --ii)
    {
      Serial.print("V");
    }
    Serial.println("");
  }
  else
  {
    Serial.print("New history:     ");
    dumpRegister(historyBuf);
    Serial.println("");
  }
}
