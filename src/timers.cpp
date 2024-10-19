#include "hwio.h"
#include "setup.h"
#include "timers.h"
#include <vector>
#include <RatFuncs.h>
#include <deque>
#include <forward_list>

// Semaphore for burning down list of callbacks from expired timers
SemaphoreHandle_t xSemaphore = xSemaphoreCreateBinary();
BaseType_t xHigherPriorityTaskWoken = pdFALSE;

// Volatile, 'cause we mess with these in ISRs
volatile uint8_t flashTimer(0);
volatile uint16_t millisTimer(0);
volatile long long elapsed(0);

hw_timer_t *timer1(nullptr);   // Timer library takes care of telling this where to point


// Holds a time variable and a callback to fire when timer expires
struct timed_callback
{
  timed_callback(unsigned long long time, std::function<void()> cb):
    expired(false),
    counter(time),
    func(cb)
  {;}

  bool expired;
  unsigned long long counter;
  std::function<void()> func;

  // Decrements counter until it reaches zero. Returns true if the call caused it to expire
  bool operator -- ()
  {
    if (expired)
    {
      return false;
    }

    --counter;
    if (counter == 0)
    {
      expired = true;
    }

    return expired;
  }

  // Need this so we can remove instances from forward_list
  bool operator == (timed_callback comp) { return this == &comp; }
};


std::forward_list<timed_callback> active_timers;
std::deque<timed_callback> expired_timers;

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
  // Handle all our hardware inputs
  for (uint8_t fd = 0; fd < 8; ++fd)
  {
    faderBank[fd]->service();
  }

  mode.service();
  gates.service();
  writeLow.service();
  writeHigh.service();

  // Handle our "blinky" timer (which indicates the current mode)
  ++millisTimer;
  ++elapsed;
  if (millisTimer == 1000)
  {
    millisTimer = 0;
  }
  flashTimer = millisTimer / 10;

  // Handle any timed_callbacks
  for (auto &timer: active_timers)
  {
    if (--timer)
    {
      expired_timers.push_back(timer);
    }
  }

  // Condition for forward_list<timed_callback>::remove_if
  active_timers.remove_if([] (const timed_callback& node) { return node.expired; });

  // Run List can't run until it gets this
  xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);
}


// Returns the elapsed time (in milliseconds) since starting Timer1
long long timestamp()
{
  cli();
  long long ret(elapsed);
  sei();
  return ret;
}


// Creates a timed_callback object with the current time.
// Timer1's ISR starts decrementing its counter until {period} milliseconds have
// elapsed. The return value is a lambda that returns true if timer is expired.
std::function<bool()> one_shot(
  long long period,
  std::function<void()> func)
{
    long long then = timestamp();
    bool expired(false);

    if (func)
    {
      active_timers.push_front(timed_callback(period, func));
    }

    return [=]() mutable {
        long long now = timestamp();
        return (now - then) >= period;
    };
}


// If Timer1 ISR gives you a semaphore, run through the list of expired timed_callbacks
// it created for you and call all their callback functions
void serviceRunList()
{
  if (xSemaphoreTake(xSemaphore, 0) == pdFALSE)
  {
    return;
  }

  for (auto timer: expired_timers)
  {
    timer.func();
    expired_timers.pop_front();
  }
}
