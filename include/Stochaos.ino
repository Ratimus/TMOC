#include <FastShiftOut.h>
// https://github.com/RobTillaart/FastShiftOut


#include <bitHelpers.h>
// https://github.com/RobTillaart/bitHelpers

const uint8_t CLK_IN(2);
const uint8_t RST_IN(3);
const uint8_t CHAOS_IN(A2);

// 74HC595 Control
const uint8_t SR_CK(5);
const uint8_t SR_LT(6);
const uint8_t SR_DS(7);

FastShiftOut FSO(SR_DS, SR_CK, MSBFIRST);

uint8_t  BCDtoDEC(0);    // CD4028 BCD to Decimal decoder
uint16_t counterONE(0);  // CHAOS CD4040 binary counter
uint16_t counterTWO(0);  // INPUT CD4040 binary counter
uint16_t flipFlop(0);    // CD40175 quad D-type flip-flops

bool chaos [] = {false, false};
bool clk   [] = {false, false};

uint8_t OUTPUT_XOR(0);

////////////////////////////////////////////////////////
//  Function prototypes
//
void printPattern(byte pattBuf);
void allTheseVoltages();


void allTheseVoltages()
{
  float DAC_ONE(0.0f);
  float DAC_TWO(0.0f);

  float CV_OUT [] = {0.0f, 0.0f, 0.0f, 0.0f};
  float Q_not [] = {0.0f, 0.0f, 0.0f, 0.0f};

  for (uint8_t ii = 0; ii < 4; ++ii)
  {
    Q_not [ii] = (float)(1.0 * !bitRead(flipFlop, ii));
  }

  float Q_D(1.0f - Q_not [3]);
  
  DAC_ONE += Q_D       * ((5.0f * 33.0f) / 100.0f);
  DAC_ONE += Q_not [0] * ((5.0f * 33.0f) / 200.0f);
  DAC_ONE += Q_not [1] * ((5.0f * 33.0f) / 300.0f);

  DAC_TWO += Q_not [0] * ((5.0f * 33.0f) / 100.0f);
  DAC_TWO += Q_not [2] * ((5.0f * 33.0f) / 200.0f);
  DAC_TWO += Q_not [3] * ((5.0f * 33.0f) / 300.0f);

  float diffMinusIN(DAC_ONE);
  float diffPlusIN(DAC_TWO);
  
  float CV_THREE(0.0f);
  float CV_FOUR(0.0f);

  float CV_SUM = diffPlusIN - diffMinusIN;
  if (CV_SUM >= 0.0f)
  {
    CV_FOUR = CV_SUM;
  }
  else
  {
    CV_THREE = CV_SUM;
  }

  CV_OUT [0] = DAC_ONE;
  CV_OUT [1] = DAC_TWO;
  CV_OUT [2] = CV_THREE;
  CV_OUT [3] = CV_FOUR;
}


void setup()
{
  pinMode(CLK_IN, INPUT_PULLUP);
  pinMode(RST_IN, INPUT_PULLUP);
  pinMode(SR_CK, OUTPUT);
  pinMode(SR_LT, OUTPUT);
  pinMode(SR_DS, OUTPUT);
  
  randomSeed(analogRead(A0));
}

void loop()
{  
  // Output of DIFF AMP to CLOCK input first CD4040

  //Simulated stochastic
  //chaos[1] = (random(1023) >  1023 / 2);
  
  //counterONE += (int)(chaos[1] && !chaos[0]);
  //chaos[0] = chaos[1];

  // RLR_DEBUG
 ++counterONE;

  // RESET IN to RST second CD4040 and to CLR of CD40175
  if (!digitalRead(RST_IN))
  {
    counterTWO = 0;
    flipFlop   = 0;
  }

  //clk[1] = !digitalRead(CLK_IN);
  clk[1] = !(millis() % 180);  // RLR_DEBUG
  
  // CLOCK IN to second CD4040
  if (clk[1] && !clk[0])
  {
    ++counterTWO;
    
    // Q10 second CD4040 triggers its own RST
    counterTWO %= 1024;

    // Q1 of second CD4040 to CLOCK CD40175
    // CD40175 outputs Q1-Q3 to CD4028 INPUTS A-C (D=0)
    if (counterTWO % 2)
    {
      BCDtoDEC = 0;
      flipFlop = 0;

      // First CD4040 Q1-Q4 to CD40175 D1-D4
      for (uint8_t ii = 0; ii < 4; ++ii)
      {
        bitWrite(flipFlop, ii, bitRead(counterONE, ii));
      }

      bitWrite(BCDtoDEC, flipFlop & 7, 1);
    }
     
    // CD4028 outputs Q0-Q7 to output gates G0-G7
    OUTPUT_XOR = 0 | BCDtoDEC;
      
    // Q2-Q9 second CD4040 to output gates (XOR or AND) G0-G7
    OUTPUT_XOR ^= ((counterTWO >> 1) & 0x00ff);

    printPattern(OUTPUT_XOR);
    allTheseVoltages();
  }
  clk[0] = clk[1];
}


void printPattern(byte pattBuf)
{
  digitalWrite(SR_LT, LOW);
  FSO.write(pattBuf & 255);
  digitalWrite(SR_LT, HIGH);
}
