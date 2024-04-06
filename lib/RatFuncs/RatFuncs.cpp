// ------------------------------------------------------------------------
// RatFuncs.cpp
//
// Oct. 2022
// Ryan "Ratimus" Richardson
// ------------------------------------------------------------------------
#include "RatFuncs.h"

//////////////////////////////////////////////////////////////////
//
//               SERIAL DEBUGGING UTILITIES
//
//////////////////////////////////////////////////////////////////
void printBuff4x4SER(uint16_t buff)
{
  if (!Serial) return;

  for (uint8_t lclRow = 0; lclRow < 4; ++lclRow)
  {
    uint8_t lclByte = buff >> (4 * lclRow);
    for (uint8_t jj = 0; jj < 4; ++jj)
    {
      Serial.print(bitRead(lclByte, (3 - jj)) ? " 1 " : " 0 ");
    }
    Serial.println("");
  }

  Serial.println("============");
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.println("");
}

void dumpBufferSER(uint16_t buff, bool eightBits /*= false*/)
{
  uint16_t tempReg = 0xffff & buff;

  if (eightBits)
  {
    for (uint8_t ii = 0; ii < 8; ++ii)
    {
      bitRead(tempReg, ii) ? Serial.print(" 1 ") : Serial.print(" 0 ");
    }

    Serial.println("");
    return;
  }
Serial.println("----------------");
  for (uint8_t ii = 0; ii <16; ++ii)
  {
    bitRead(tempReg, ii) ? Serial.print("1 ") : Serial.print("0 ");
  }

  Serial.println("");
}



//////////////////////////////////////////////////////////////////
//
//                  HW INTERFACE STUFF
//
//////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////
//
//               COMPUTATIONAL HELPERS
//
//////////////////////////////////////////////////////////////////

// Adds deltaN to N.
// Constrains N to the interval [nMin : nMax] (both ends inclusive).
// Wraps on overflow (rather than clipping)
void wrapConstrain(int16_t * pN, int16_t nMin, int16_t nMax)
{
  *pN = wrapConstrain(*pN, nMin, nMax);
}

void wrapConstrain(int8_t * pN, int16_t nMin, int16_t nMax)
{
  int16_t pN16(*pN);
  wrapConstrain(&pN16, nMin, nMax);
  *pN = (int8_t)(pN16 & 0xFF);
}

int16_t wrapConstrain(int16_t N, int16_t dN, int16_t nMin, int16_t nMax)
{
  if (nMin > nMax) return wrapConstrain(N - dN, nMax, nMin);
  else return wrapConstrain(N + dN, nMin, nMax);
}

int16_t wrapConstrain(int16_t N, int16_t nMin, int16_t nMax)
{
  if ((nMin <= N) && (N <= nMax)) { return N; }

  int16_t range = nMax - nMin + 1;
  if (range == 0) { return nMax; }
  N = ((N - nMin) % range);
  if (N < 0)
  {
    N += (nMax + 1);
  }
  else
  {
    N += nMin;
  }
  return N;
}

