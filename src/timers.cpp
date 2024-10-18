#include "hwio.h"
#include "setup.h"
#include "timers.h"
#include <vector>
#include <forward_list>


// Volatile, 'cause we mess with these in ISRs
volatile uint8_t flashTimer(0);
volatile uint16_t millisTimer(0);
volatile long long elapsed(0);

hw_timer_t *timer1(nullptr);   // Timer library takes care of telling this where to point

struct timed_callback
{
  explicit timed_callback(unsigned long long time, std::function<void()>*cb):
    counter(time),
    func(cb)
  { ; }

  unsigned long long counter;
  std::function<void()>*func;

  bool operator -- () { return tick(); }
  bool tick()
  {
    if (counter)
    {
      --counter;
    }

    return !counter;
  }

  bool operator == (timed_callback comp) { return this == &comp; }
};

std::forward_list<timed_callback> callbacks;
std::vector<std::function<void()>*> runlist;

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
  cli();
  uint8_t timerVal = flashTimer;
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
  ++elapsed;
  if (millisTimer == 1000)
  {
    millisTimer = 0;
  }
  flashTimer = millisTimer / 10;

  auto iter = callbacks.begin();
  while (iter != callbacks.cend())
  {
    if (!iter->tick())
    {
      runlist.push_back(iter->func);
      callbacks.remove(*iter);
      ++iter;
    }
  }
}

long long timestamp()
{
  cli();
  long long ret(elapsed);
  sei();
  return ret;
}

/*
for counter, func in callbacks.items()
  --counter;
  if !counter
    callbacks.pop(func)
*/

std::function<bool()> one_shot(
  long long period,
  std::function<void()>*func)
{
    long long then = timestamp();
    bool expired(false);

    if (func)
    {
      callbacks.push_front(timed_callback(period, func));
    }

    return [=]() mutable {
        long long now = timestamp();
        return (now - then) >= period;
    };
}

void serviceRunList()
{
  while (!runlist.empty())
  {
    std::function<void()>func = *runlist.back();
    func();
    runlist.pop_back();
  }
}


