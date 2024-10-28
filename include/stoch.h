#ifndef STOCH_DOT_H
#define STOCH_DOT_H

#include <Arduino.h>
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

  mutable bool bitSetPending_;
  mutable bool bitClearPending_;

  bool stochasticize(const bool startVal) const;
};

#endif