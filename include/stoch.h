#include <ESP32AnalogRead.h>

struct Stochasticizer
{
  float THRESH_LOW;
  float THRESH_HIGH;

  ESP32AnalogRead& loop;
  ESP32AnalogRead& cv;

  Stochasticizer(
    ESP32AnalogRead& extLoop,
    ESP32AnalogRead& extCv);

  bool stochasticize(const bool startVal) const;
};

