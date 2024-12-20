#include "timers.h"
#include <vector>
#include <RatFuncs.h>
#include <deque>
#include <forward_list>
#include "hwio.h"

const uint16_t uS_TO_mS(1000);
const uint16_t ONE_KHZ_MICROS(uS_TO_mS); // 1000 uS for 1khz timer cycle for encoder

// Volatile, 'cause we mess with these in ISRs
volatile uint8_t   flashTimer  = 0;
volatile uint16_t  millisTimer = 0;
volatile long long elapsed     = 0;

hw_timer_t *timer1(nullptr);   // Timer library takes care of telling this where to point

void ICACHE_RAM_ATTR onTimer1();
void serviceRunList();

BaseType_t        xHigherPriorityTaskWoken = pdFALSE;
TaskHandle_t      callbacksTaskHandle(NULL);
SemaphoreHandle_t callbacks_sem;


void IRAM_ATTR callbacksTask(void *param)
{
  callbacks_sem = xSemaphoreCreateBinary();

  while (1)
  {
    xSemaphoreTake(callbacks_sem, portMAX_DELAY);
    serviceRunList();
  }
}


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
std::forward_list<timed_callback> expired_timers;

void setupTimers()
{
  // Set up master clock
  timer1 = timerBegin(1, 80, true);
  timerAttachInterrupt(timer1, &onTimer1, true);
  timerAlarmWrite(timer1, ONE_KHZ_MICROS, true);
  timerAlarmEnable(timer1);

  xTaskCreate
  (
    callbacksTask,
    "serviceRunList Task",
    4096,
    NULL,
    10,
    &callbacksTaskHandle
  );
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
  serviceIO();

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
    if (--timer && timer.func != nullptr)
    {
      expired_timers.push_front(timer);
    }
  }

  // Pull expired timers off the list
  active_timers.remove_if([] (const timed_callback& node) { return node.expired; });

  // Run List can't run until it gets this
  xSemaphoreGiveFromISR(callbacks_sem, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR();
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

    active_timers.push_front(timed_callback(period, func));

    auto isExpired_lambda = [=]() mutable
    {
        long long now = timestamp();
        return (now - then) >= period;
    };

    return isExpired_lambda;
}


// Run through the list of expired timed_callbacks invoke their callback functions
// NB: This is meant to be called from a FreeRTOS task!
void serviceRunList()
{
    if (expired_timers.empty())
    {
      return;
    }

    // timed_callbacks are added with push_front - reverse the list so it's FIFO
    expired_timers.reverse();
    for (auto timer: expired_timers)
    {
      timer.func();
    }

    expired_timers.clear();
}
