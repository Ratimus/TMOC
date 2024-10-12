#include "hwio.h"
#include "setup.h"
#include "timers.h"
#include "CtrlPanel.h"

// Volatile, 'cause we mess with these in ISRs
volatile uint8_t flashTimer(0);
volatile uint16_t millisTimer(0);

hw_timer_t *timer1(nullptr);   // Timer library takes care of telling this where to point

void ICACHE_RAM_ATTR onTimer1();

void setupTimers()
{
    // Set up master clock
    timer1 = timerBegin(1, 80, true);
    timerAttachInterrupt(timer1, &onTimer1, true);
    timerAlarmWrite(timer1, ONE_KHZ_MICROS, true);
    timerAlarmEnable(timer1);
}

// This blinks LEDs on and off with different timings to indicate which mode you're in
// SOLID: set length (get here with a single click)
// FAST: select pattern to load (get here with a double-click)
// SLOW: select slot for saving current pattern (get here with a long press)
uint8_t getFlashTimer()
{
  uint8_t timerVal;
  cli();
  timerVal = flashTimer;
  sei();
  uint8_t retval(0);
  if ((timerVal / 4) % 2)
  {
    bitWrite(retval, 0, 1);
  }

  if ((timerVal / 25) % 2)
  {
    bitWrite(retval, 1, 1);
  }
  return retval;
}

////////////////////////////
// ISR for Timer 1
void ICACHE_RAM_ATTR onTimer1()
{
  for (uint8_t fd = 0; fd < 8; ++fd)
  {
    faderBank[fd]->service();
  }

  mode.service();
  gates.service();
  writeLow.service();
  writeHigh.service();

  ++millisTimer;
  if (millisTimer == 1000)
  {
    millisTimer = 0;
  }
  flashTimer = millisTimer / 10;
}
