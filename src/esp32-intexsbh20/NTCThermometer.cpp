/*
 * project:  Intex PureSpa WiFi Controller
 *
 * file:     NTCThermometer.cpp
 *
 * encoding: UTF-8
 * created:  23rd March 2021
 *
 * Copyright (C) 2021 Jens B.
 *
 * Enhanced and maintained by Petr Kašpar (27 March 2026).
 * https://github.com/caspercze
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "NTCThermometer.h"
#include "common.h"
#include <math.h>


/**
 * @param refResistance resistance between NTC and GND [Ohm]
 * @param refVoltage voltage at NTC [V]
 * @param adcScale inverted voltage divider relation of analog input (Wemos D1 mini: 320/100)
 */
void NTCThermometer::setup(unsigned int refResistance, float refVoltage, float adcScale)
{
  this->refResistance = refResistance;
  this->refVoltage = refVoltage;
  analogReadResolution(12);
  // Do NOT call analogSetPinAttenuation() here: on Arduino-ESP32 v5.x it
  // drives the pin LOW (output mode) instead of configuring ADC attenuation,
  // which causes analogRead() to return near-zero counts.
  // analogRead() itself configures the pin as ADC input with the correct
  // attenuation (ADC_ATTEN_DB_12, 0–3.3 V) on first call.
  // adcScale: accounts for an external voltage divider between the NTC circuit
  // and the ADC pin. On ESP8266 D1 Mini the divider was 320k/100k (adcScale=3.2).
  // On ESP32-S3 the ADC reads 0-3.3 V directly, so if the PCB still has the
  // same divider, pass adcScale = (R_top + R_bot) / R_bot (e.g. 420.f/100.f).
  // Without any external divider use adcScale = 1.0f.
  // The factor scales the raw ADC reading back to represent the undivided voltage.
  this->adcScale = (refVoltage * adcScale) / 4095.0f;

  for (unsigned int i = 0; i < HISTORY_DEPTH; ++i)
  {
    (void)getTemperature();
  }
}

/**
 * read analog input multiple times and return average [millivolts]
 *
 * Uses analogRead() (raw 0–4095) and converts using the known reference
 * voltage. analogReadMilliVolts() returns 0 on Arduino-ESP32 v5.x when
 * the internal eFuse calibration path fails silently.
 *
 * @return average millivolts
 */
float NTCThermometer::analogReadMultiple()
{
  long sum = 0;
  for (unsigned int i = 0; i < CONSECUTIVE_SAMPLES; i++)
  {
    sum += analogRead(PIN::NTC);
  }
  // Convert raw counts (0–4095) to millivolts using the ADC reference voltage.
  return ((float)sum / CONSECUTIVE_SAMPLES) * (refVoltage * 1000.0f / 4095.0f);
}

/**
 * measure resistance R of a voltage divider:
 * Vref - R (NTC, var) - Rref - GND
 *
 * return resistance [Ohm]
 */
int NTCThermometer::getResistance()
{
  float vMv = analogReadMultiple();           // mV at ADC pin
  if (vMv <= 0.0f) return (int)refResistance; // guard: avoid division by zero
  float v = vMv / 1000.0f;                   // convert to V
  return round((refVoltage / v - 1) * refResistance);
}

/**
 * sample current 10k NTC temperature [°C] and calculate block moving average
 *
 * see Arduino Playground: "Reading a Thermistor"
 *
 * @return temperature [°C]
 */
float NTCThermometer::getTemperature()
{
  // calculate current temperature and
  // update temperature history ring buffer
  float x = log(getResistance());
  history[historyHead++] = 1.0f / (0.001129148f + 0.000234125f*x + 0.0000000876741f*x*x*x) - 273.15f;
  historyHead %= HISTORY_DEPTH;
  if (historyDepth < HISTORY_DEPTH)
  {
    historyDepth++;
  }

  // calculate block moving average
  float sum = 0;
  for (unsigned int i=0; i<historyDepth; i++)
  {
    sum += history[i];
  }

  return sum/historyDepth;
}
