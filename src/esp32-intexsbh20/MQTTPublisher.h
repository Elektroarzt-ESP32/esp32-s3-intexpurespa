/*
 * project:  Intex PureSpa WiFi Controller
 *
 * file:     MQTTPublisher.h
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

#ifndef MQTT_PUBLISHER_H
#define MQTT_PUBLISHER_H

#include <stdint.h>
#include "common.h"

class MQTTClient;
class PureSpaIO;
class NTCThermometer;


class MQTTPublisher
{
public:
  MQTTPublisher(MQTTClient& mqttClient, PureSpaIO& PureSpaIO, NTCThermometer& thermometer);

public:
  void setRetainAll(bool retain);
  bool isRetainAll() const;

public:
  void loop();
  /** Publish controller temp, IP, RSSI now (call right after MQTT connects). */
  void publishWifiDiagnosticsNow();
  bool hasInitialSwitchStatesPublished() const;
  bool hasInitialCoreSwitchStatesPublished() const;
  bool hasInitialClimateStatesPublished() const;

private:
  static const unsigned int BUFFER_SIZE = 16;

private:
  void publish(const char* topic, int i);
  void publish(const char* topic, unsigned int u);

  void publishIfDefined(const char* topic, uint8 b, uint8 undef);
  void publishIfDefined(const char* topic, uint16 i, uint16 undef);
  void publishIfDefined(const char* topic, int i, int undef);

  void publishTemp(const char* topic, float t, bool forcePublish = false);
  void publishWifiDiagnostics(unsigned long now);

private:
  MQTTClient& mqttClient;
  PureSpaIO& pureSpaIO;
  NTCThermometer& thermometer;
  bool retainAll;

private:
  unsigned long poolUpdateTime = 0;
  unsigned long poolStateUpdateTime = 0;
  unsigned long wifiStateUpdateTime = 0;
  bool initialSwitchStatesPublished = false;
  bool initialBubblePublished = false;
  bool initialFilterPublished = false;
  bool initialPowerPublished = false;
  bool initialHeaterPublished = false;
  bool initialJetPublished = false;
  bool initialDisinfectionPublished = false;
  bool initialClimateStatesPublished = false;
  char buf[BUFFER_SIZE];

};

#endif /* MQTT_PUBLISHER_H */

