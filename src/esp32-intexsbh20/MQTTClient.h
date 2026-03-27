/*
 * project:  Intex PureSpa WiFi Controller
 *
 * file:     MQTTClient.h
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

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <functional>
#include <map>
#include <WiFi.h>
#include <PubSubClient.h>
#include "common.h"

class MQTTClient
{
public:
  MQTTClient() : mqttClient(wifiClient) {}

public:
  void addMetadata(const char* topic, const char* message);
  void addSubscriber(const char* topic, void (*setter)(bool value));
  void addSubscriber(const char* topic, void (*setter)(int value));

  void setup(const char* mqttServer, uint16 mqttPort, const char* mqttUsername, const char* mqttPassword,
             const char* clientId, const char* willTopic, const char* willMessage);
  void configureDiscovery(const char* prefix, bool enabled);
  void loop();

  bool isConnected();
  bool publish(const char* topic, const String& payload, bool retain=false, bool force=false);

private:
  static const unsigned int RECONNECT_DELAY = 3000;

  void subscriptionUpdate(char* topic, byte* message, unsigned int length);
  void reconnect();
  void publishHADiscovery();

private:
  PubSubClient mqttClient;
  WiFiClient wifiClient;
  const char* clientId = nullptr;
  const char* willTopic = nullptr;
  const char* willMessage = nullptr;
  const char* mqttuser = nullptr;
  const char* mqttpw = nullptr;

  std::map<String, String> metadata;
  std::map<String, String> publications;
  std::map<String, std::function<void (bool)>> boolSubscriber;
  std::map<String, std::function<void (int)>> intSubscriber;

  String discoveryPrefix = CONFIG::DEFAULT_MQTT_DISCOVERY;
  bool discoveryEnabled = true;

  unsigned int now = 0;
  unsigned int lastConnectTime = 0;
};

#endif /* MQTT_CLIENT_H */
