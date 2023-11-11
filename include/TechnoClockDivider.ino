
#include <EEPROM.h>

#define BUTTON_PIN         12
#define CLOCK_PIN          13
#define OUT_PIN1           5
#define OUT_PIN2           8
#define OUT_PIN3           6
#define OUT_PIN4           9
#define OUT_PIN5           7
#define OUT_PIN6           10
#define RESET_PIN          11
#define LONG_PRESS         600
#define SHORT_PRESS        10
#define MAX_STEPS          8

bool alt_positions = false;
int active_steps1 = 1;
int active_steps2 = 1;
int active_steps3 = 1;
int active_steps4 = 1;
int active_steps5 = 1;
int active_steps6 = 1;
int offset        = 0;
int counter       = -1;
int steps         = MAX_STEPS;
bool positions1[MAX_STEPS] = {};
bool positions2[MAX_STEPS] = {};

bool send_tick          = false;
bool clock_state        = false;
bool last_clock_state   = false;
bool last_button_state  = false;
bool last_reset_state   = false;
int button_pressed_time = 0;
int long_pressed_time = 0;

bool getPosition(uint8_t index, bool positions[MAX_STEPS])
{
  int step_idx = (index + offset) % steps;
  return positions[step_idx];
}

void checkButton() {
  bool button_state = digitalRead(BUTTON_PIN) == LOW;
  if (!button_state && last_button_state == button_state) return;
  int now = millis();

  // Save the time when the button was pressed
  if (button_state && !last_button_state)
  {
    button_pressed_time = now;
    long_pressed_time = now;
  }
  last_button_state = button_state;

  int press_time = now - button_pressed_time;

  // On long press
  if (button_state && press_time >= LONG_PRESS)
  {
    if (now - long_pressed_time >= LONG_PRESS)
    {
      //offset += 1;
      //if (offset >= steps) offset = 0;

      alt_positions = !alt_positions;

      long_pressed_time = now;    
    }
    return;
  }

  // On button up after a short press
  if (!button_state && press_time >= SHORT_PRESS && press_time < LONG_PRESS)
  {
    if (alt_positions)
    {
      active_steps2 += 1;
      if (active_steps2 > steps) active_steps2 = 1;
      setPositions(active_steps2, positions2);
      EEPROM.write(1, active_steps2);
    }
    else
    {
      active_steps1 += 1;
      if (active_steps1 > steps) active_steps1 = 1;
      setPositions(active_steps1, positions1);
      EEPROM.write(0, active_steps1);
    }
    return;
  }
}

void checkReset()
{
  bool reset = !digitalRead(RESET_PIN);
  if (reset == last_reset_state) return;
  last_reset_state = reset;
  if (reset) counter = 0;
}

void setPositions(int active_steps, bool positions[MAX_STEPS])
{
  for (int i = 0; i < steps; i++)
  {
    positions[i] = false;
  }

  if (active_steps == 1 || steps - active_steps <= 1)
  {
    for (int i = 0; i < active_steps; i++)
    {
      positions[i] = true;
    }
    return;
  }

  int remainder = active_steps;
  int quotient = 0;
  int skip = 0;

  while (remainder > 0)
  {
    quotient = floor(steps / remainder);
    int rem = steps % remainder;
    if (rem) quotient += 1;

    for (int i = 0; i < steps; i++)
    {
      if ((i + 1) % quotient == 0)
      {
        positions[(i + skip) % steps] = true;
      }
    }

    skip += 1;
    remainder -= floor(steps / quotient);
  }
}


void onClockOn()
{
  counter += 1;
  if (counter >= steps) counter = 0;

  bool is_active1 = getPosition(counter, positions1);
  digitalWrite(OUT_PIN1, is_active1);

  bool is_active2 = getPosition(counter, positions2);
  digitalWrite(OUT_PIN2, is_active2);

  setLed(counter, alt_positions ? !is_active2 : !is_active1);
}

void onClockOff()
{
  digitalWrite(OUT_PIN1, LOW);
  digitalWrite(OUT_PIN2, LOW);
}

void pciSetup(byte pin)
{
  *digitalPinToPCMSK(pin) |= bit (digitalPinToPCMSKbit(pin));  // enable pin
  PCIFR  |= bit (digitalPinToPCICRbit(pin)); // clear any outstanding interrupt
  PCICR  |= bit (digitalPinToPCICRbit(pin)); // enable interrupt for the group
}

ISR(PCINT0_vect)
{
  send_tick = true;
}

void setup()
{
  pinMode(CLOCK_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(OUT_PIN1, OUTPUT);
  pinMode(OUT_PIN2, OUTPUT);
  pinMode(OUT_PIN3, OUTPUT);
  pinMode(OUT_PIN4, OUTPUT);
  pinMode(OUT_PIN5, OUTPUT);
  pinMode(OUT_PIN6, OUTPUT);

  pciSetup(CLOCK_PIN);

  active_steps1 = EEPROM.read(0);
  active_steps2 = EEPROM.read(1);
  active_steps3 = EEPROM.read(2);
  active_steps4 = EEPROM.read(3);
  active_steps5 = EEPROM.read(4);
  active_steps6 = EEPROM.read(5);

  setPositions(active_steps1, positions1);
  setPositions(active_steps2, positions2);
  setPositions(active_steps3, positions3);
  setPositions(active_steps4, positions4);
  setPositions(active_steps5, positions5);
  setPositions(active_steps6, positions6);
}

void loop()
{
  checkButton();
  checkReset();

  if (!send_tick) return;
  send_tick = false;

  clock_state = digitalRead(CLOCK_PIN);
  if (clock_state == last_clock_state) return;
  last_clock_state = clock_state;

  clock_state ? onClockOn() : onClockOff();
}
