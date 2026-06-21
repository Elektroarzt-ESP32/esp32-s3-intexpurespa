/*
 * project:  Intex PureSpa WiFi Controller
 *
 * file:     MQTTClient.cpp
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

#include "MQTTClient.h"

#include "common.h"
#include "OTAUpdate.h"
#include "ConfigurationFile.h"
#include <stdint.h>

extern OTAUpdate otaUpdate;
extern ConfigurationFile config;

static String sanitizeDiscoveryId(const char* value)
{
  String out;
  if (!value)
  {
    return String("intex_purespa");
  }

  for (const char* p = value; *p; ++p)
  {
    char c = *p;
    if (isalnum((unsigned char)c))
    {
      out += (char)tolower((unsigned char)c);
    }
    else if (c == '_' || c == '-')
    {
      out += c;
    }
    else
    {
      out += '_';
    }
  }

  while (out.indexOf("__") >= 0)
  {
    out.replace("__", "_");
  }
  if (out.length() == 0)
  {
    out = "intex_purespa";
  }
  return out;
}

void MQTTClient::subscriptionUpdate(char* topic, byte* message, unsigned int length)
{
  char payload[128];
  unsigned int copyLen = (length < sizeof(payload) - 1) ? length : (sizeof(payload) - 1);
  memcpy(payload, message, copyLen);
  payload[copyLen] = '\0';

  if (strcmp(topic, MQTT_TOPIC::CMD_OTA) == 0)
  {
    DEBUG_MSG("set %s: %s\n", topic, payload);
    const char* url = config.get(CONFIG_TAG::WIFI_OTA_URL);
    if (url && *url)
    {
      otaUpdate.start(url, *this);
    }
    return;
  }
  else if (strcmp(topic, MQTT_TOPIC::CMD_RESTART) == 0)
  {
    DEBUG_MSG("set %s: %s\n", topic, payload);
    publish(MQTT_TOPIC::OTA, F("restart requested"), false, true);
    delay(100);
    ESP.restart();
    return;
  }
  else if (strcmp(topic, MQTT_TOPIC::CMD_REPUBLISH) == 0)
  {
    DEBUG_MSG("set %s: %s\n", topic, payload);
    publishHADiscovery();
    return;
  }

  // Erase the cache entry for the corresponding state topic BEFORE invoking
  // the subscriber callback.  The callback (e.g. CMD_POWER) calls publish()
  // with force=true, which writes a new cache entry.  If we erased AFTER the
  // callback (original order) that cache entry would be destroyed immediately,
  // causing MQTTPublisher to see "no cached value" on its next poll and
  // re-publish a potentially transient panel reading → flicker in HA.
  String t = topic;
  String c = "command/";
  int p = t.indexOf(c);
  if (p >= 0)
  {
    t.remove(p, c.length());
    publications.erase(t);
  }
  publications.erase(willTopic);

  auto ib = boolSubscriber.find(topic);
  if (ib != boolSubscriber.end())
  {
    // Home Assistant MQTT climate sends "heat"/"off" for hvac modes.
    // For our boolean topics we treat "heat" like "on".
    bool on = (strcmp("on", payload) == 0)
               || (strcmp("heat", payload) == 0)
               || (strcmp("heating", payload) == 0);
    DEBUG_MSG("set %s: %d\n", topic, on);
    ib->second(on);
  }
  else
  {
    auto ii = intSubscriber.find(topic);
    if (ii != intSubscriber.end())
    {
      int value = atoi(payload);
      DEBUG_MSG("set %s: %d\n", topic, value);
      ii->second(value);
    }
  }
}

void MQTTClient::setup(const char* mqttServer, uint16 mqttPort, const char* mqttUsername, const char* mqttPassword, const char* cid, const char* wt, const char* wm)
{
  mqttuser = mqttUsername;
  mqttpw = mqttPassword;
  clientId = cid;
  willTopic = wt;
  willMessage = wm;

  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setBufferSize(1024);
  mqttClient.setKeepAlive(25);
  mqttClient.setSocketTimeout(15);
  mqttClient.setCallback(std::bind(&MQTTClient::subscriptionUpdate, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void MQTTClient::reconnect()
{
  if (!mqttClient.connected() && timeDiff(now, lastConnectTime) > RECONNECT_DELAY)
  {
    Serial.print("trying to connect to MQTT server ... ");
    if (mqttClient.connect(clientId, mqttuser, mqttpw, willTopic, MQTTQOS0, true, willMessage))
    {
      Serial.println("success");

      if (!lastConnectTime)
      {
        for (auto m: metadata)
        {
          mqttClient.publish(m.first.c_str(), m.second.c_str(), true);
        }
      }

      publishHADiscovery();

      for (auto s: boolSubscriber)
      {
        mqttClient.subscribe(s.first.c_str());
      }
      for (auto s: intSubscriber)
      {
        mqttClient.subscribe(s.first.c_str());
      }
      mqttClient.subscribe(MQTT_TOPIC::CMD_OTA);
      mqttClient.subscribe(MQTT_TOPIC::CMD_RESTART);
      mqttClient.subscribe(MQTT_TOPIC::CMD_REPUBLISH);
    }
    else
    {
      Serial.printf("failed, rc=%d\n", mqttClient.state());
    }
    lastConnectTime = now;
  }
}

void MQTTClient::configureDiscovery(const char* prefix, bool enabled)
{
  discoveryEnabled = enabled;
  discoveryPrefix = (prefix && *prefix) ? prefix : CONFIG::DEFAULT_MQTT_DISCOVERY;
}

void MQTTClient::loop()
{
  now = millis();
  if (!mqttClient.connected())
  {
    reconnect();
  }
  mqttClient.loop();
}

void MQTTClient::addMetadata(const char* topic, const char* message)
{
  metadata[topic] = message;
}

void MQTTClient::addSubscriber(const char* topic, void (*setter)(bool value))
{
  boolSubscriber[topic] = setter;
}

void MQTTClient::addSubscriber(const char* topic, void (*setter)(int value))
{
  intSubscriber[topic] = setter;
}

bool MQTTClient::isConnected()
{
  return mqttClient.connected();
}

bool MQTTClient::publish(const char* topic, const String& message, bool retain, bool force)
{
  if (lastConnectTime)
  {
    auto i = publications.find(topic);
    bool changed = (i == publications.end()) ? true : !i->second.equals(message);
    if (mqttClient.connected() && (changed || force))
    {
      bool published = mqttClient.publish(topic, message.c_str(), retain);
      if (published && changed)
      {
        publications[topic] = message;
      }
      return published;
    }
  }
  return false;
}

void MQTTClient::publishHADiscovery()
{
  if (!discoveryEnabled)
  {
    return;
  }

  char topic[192];
  char payload[1152];

  String nodeId = sanitizeDiscoveryId(clientId);
  const char* devName = clientId;
  const char* model = clientId;
  auto itModel = metadata.find(MQTT_TOPIC::MODEL);
  if (itModel != metadata.end())
  {
    model = itModel->second.c_str();
  }

  const char* version = CONFIG::WIFI_VERSION;
  auto itVersion = metadata.find(MQTT_TOPIC::VERSION);
  if (itVersion != metadata.end())
  {
    version = itVersion->second.c_str();
  }

  String configUrl = "http://" + WiFi.localIP().toString();

  auto publishCfg = [&](const char* component, const char* objectId, const char* json)
  {
    snprintf(topic, sizeof(topic), "%s/%s/%s/%s/config", discoveryPrefix.c_str(), component, nodeId.c_str(), objectId);
    mqttClient.publish(topic, json, true);
  };

  snprintf(payload, sizeof(payload),
    "{\"name\":\"Power\",\"uniq_id\":\"%s_power\",\"stat_t\":\"%s\",\"cmd_t\":\"%s\",\"pl_on\":\"on\",\"pl_off\":\"off\",\"icon\":\"mdi:power\",\"avty_t\":\"%s\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\",\"mf\":\"Intex\",\"mdl\":\"%s\",\"sw\":\"%s\",\"cu\":\"%s\"}}",
    nodeId.c_str(), MQTT_TOPIC::POWER, MQTT_TOPIC::CMD_POWER, MQTT_TOPIC::STATE, nodeId.c_str(), devName, model, version, configUrl.c_str());
  publishCfg("switch", "power", payload);

  snprintf(payload, sizeof(payload),
    "{\"name\":\"Filter\",\"uniq_id\":\"%s_filter\",\"stat_t\":\"%s\",\"cmd_t\":\"%s\",\"pl_on\":\"on\",\"pl_off\":\"off\",\"icon\":\"mdi:air-filter\",\"avty_t\":\"%s\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\",\"dev\":{\"ids\":[\"%s\"],\"cu\":\"%s\"}}",
    nodeId.c_str(), MQTT_TOPIC::FILTER, MQTT_TOPIC::CMD_FILTER, MQTT_TOPIC::STATE, nodeId.c_str(), configUrl.c_str());
  publishCfg("switch", "filter", payload);

  snprintf(payload, sizeof(payload),
    "{\"name\":\"Bubble\",\"uniq_id\":\"%s_bubble\",\"stat_t\":\"%s\",\"cmd_t\":\"%s\",\"pl_on\":\"on\",\"pl_off\":\"off\",\"icon\":\"mdi:chart-bubble\",\"avty_t\":\"%s\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\",\"dev\":{\"ids\":[\"%s\"],\"cu\":\"%s\"}}",
    nodeId.c_str(), MQTT_TOPIC::BUBBLE, MQTT_TOPIC::CMD_BUBBLE, MQTT_TOPIC::STATE, nodeId.c_str(), configUrl.c_str());
  publishCfg("switch", "bubble", payload);

  snprintf(payload, sizeof(payload),
    "{\"name\":\"Heater\",\"uniq_id\":\"%s_heater\",\"stat_t\":\"%s\",\"cmd_t\":\"%s\",\"pl_on\":\"on\",\"pl_off\":\"off\",\"stat_on\":\"on\",\"stat_off\":\"off\",\"icon\":\"mdi:heat-wave\",\"avty_t\":\"%s\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\",\"dev\":{\"ids\":[\"%s\"],\"cu\":\"%s\"}}",
    nodeId.c_str(), MQTT_TOPIC::HEATER, MQTT_TOPIC::CMD_HEATER, MQTT_TOPIC::STATE, nodeId.c_str(), configUrl.c_str());
  publishCfg("switch", "heater", payload);

  // MQTT climate entity for controlling heater + setpoint via HA.
  // Modes:
  //  - "heat": ensure power on + heater on
  //  - "off": heater off (power remains as-is)
  snprintf(payload, sizeof(payload),
    "{\"name\":\"SPA Heater Thermostat\",\"uniq_id\":\"%s_climate\",\"topic\":\"%s\",\"modes\":[\"off\",\"heat\"],"
    "\"mode_command_topic\":\"%s\",\"mode_state_topic\":\"%s\","
    "\"action_topic\":\"%s\","
    "\"optimistic\":false,"
    "\"temperature_command_topic\":\"%s\",\"temperature_state_topic\":\"%s\",\"current_temperature_topic\":\"%s\","
    "\"temperature_unit\":\"C\",\"min_temp\":20,\"max_temp\":40,\"temp_step\":1,"
    "\"payload_available\":\"online\",\"payload_not_available\":\"offline\","
    "\"availability_topic\":\"%s\","
    "\"icon\":\"mdi:thermostat\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\",\"mf\":\"Intex\",\"mdl\":\"%s\",\"sw\":\"%s\",\"cu\":\"%s\"}}",
    nodeId.c_str(),
    MQTT_TOPIC::STATE,
    MQTT_TOPIC::CMD_HEATER,
    MQTT_TOPIC::CLIMATE_MODE,
    MQTT_TOPIC::CLIMATE_ACTION,
    MQTT_TOPIC::CMD_WATER,
    MQTT_TOPIC::WATER_SET,
    MQTT_TOPIC::WATER_ACT,
    MQTT_TOPIC::STATE,
    nodeId.c_str(), devName, model, version, configUrl.c_str());
  publishCfg("climate", "climate", payload);

  snprintf(payload, sizeof(payload),
    "{\"name\":\"Water Temp\",\"uniq_id\":\"%s_water_temp\",\"stat_t\":\"%s\",\"dev_cla\":\"temperature\",\"unit_of_meas\":\"°C\",\"icon\":\"mdi:water-thermometer\",\"avty_t\":\"%s\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\","
    "\"dev\":{\"ids\":[\"%s\"],\"cu\":\"%s\"}}",
    nodeId.c_str(), MQTT_TOPIC::WATER_ACT, MQTT_TOPIC::STATE, nodeId.c_str(), configUrl.c_str());
  publishCfg("sensor", "water_temp", payload);

  snprintf(payload, sizeof(payload),
    "{\"name\":\"State\",\"uniq_id\":\"%s_state\",\"stat_t\":\"%s\",\"icon\":\"mdi:hot-tub\",\"dev\":{\"ids\":[\"%s\"],\"cu\":\"%s\"}}",
    nodeId.c_str(), MQTT_TOPIC::STATE, nodeId.c_str(), configUrl.c_str());
  publishCfg("sensor", "state", payload);

  snprintf(payload, sizeof(payload),
    "{\"name\":\"Error\",\"uniq_id\":\"%s_error\",\"stat_t\":\"%s\",\"icon\":\"mdi:alert-circle-outline\",\"avty_t\":\"%s\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\","
    "\"dev\":{\"ids\":[\"%s\"],\"cu\":\"%s\"}}",
    nodeId.c_str(), MQTT_TOPIC::ERROR, MQTT_TOPIC::STATE, nodeId.c_str(), configUrl.c_str());
  publishCfg("sensor", "error", payload);

  snprintf(payload, sizeof(payload),
    "{\"name\":\"WiFi RSSI\",\"uniq_id\":\"%s_rssi\",\"stat_t\":\"%s\",\"dev_cla\":\"signal_strength\",\"unit_of_meas\":\"dBm\",\"icon\":\"mdi:wifi\",\"ent_cat\":\"diagnostic\","
    "\"avty_t\":\"%s\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\","
    "\"dev\":{\"ids\":[\"%s\"],\"cu\":\"%s\"}}",
    nodeId.c_str(), MQTT_TOPIC::RSSI, MQTT_TOPIC::STATE, nodeId.c_str(), configUrl.c_str());
  publishCfg("sensor", "wifi_rssi", payload);

  snprintf(payload, sizeof(payload),
    "{\"name\":\"WiFi IP\",\"uniq_id\":\"%s_wifi_ip\",\"stat_t\":\"%s\","
    "\"icon\":\"mdi:ip-network\",\"ent_cat\":\"diagnostic\","
    "\"avty_t\":\"%s\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\","
    "\"dev\":{\"ids\":[\"%s\"],\"cu\":\"%s\"}}",
    nodeId.c_str(), MQTT_TOPIC::IP, MQTT_TOPIC::STATE, nodeId.c_str(), configUrl.c_str());
  publishCfg("sensor", "wifi_ip", payload);

  snprintf(payload, sizeof(payload),
    "{\"name\":\"Controller Temp\",\"uniq_id\":\"%s_ctrl_temp\",\"stat_t\":\"%s\",\"dev_cla\":\"temperature\",\"unit_of_meas\":\"°C\",\"icon\":\"mdi:memory\",\"ent_cat\":\"diagnostic\","
    "\"avty_t\":\"%s\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\","
    "\"dev\":{\"ids\":[\"%s\"],\"cu\":\"%s\"}}",
    nodeId.c_str(), MQTT_TOPIC::WIFI_TEMP, MQTT_TOPIC::STATE, nodeId.c_str(), configUrl.c_str());
  publishCfg("sensor", "controller_temp", payload);

  if (metadata.find(MQTT_TOPIC::JET) != metadata.end())
  {
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Jet\",\"uniq_id\":\"%s_jet\",\"stat_t\":\"%s\",\"cmd_t\":\"%s\",\"pl_on\":\"on\",\"pl_off\":\"off\",\"icon\":\"mdi:waves\",\"avty_t\":\"%s\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\",\"dev\":{\"ids\":[\"%s\"],\"cu\":\"%s\"}}",
      nodeId.c_str(), MQTT_TOPIC::JET, MQTT_TOPIC::CMD_JET, MQTT_TOPIC::STATE, nodeId.c_str(), configUrl.c_str());
    publishCfg("switch", "jet", payload);
  }

  if (metadata.find(MQTT_TOPIC::DISINFECTION) != metadata.end())
  {
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Disinfection\",\"uniq_id\":\"%s_disinfection\",\"stat_t\":\"%s\",\"cmd_t\":\"%s\",\"min\":0,\"max\":8,\"mode\":\"box\",\"step\":1,\"unit_of_meas\":\"h\",\"icon\":\"mdi:shimmer\",\"avty_t\":\"%s\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\",\"dev\":{\"ids\":[\"%s\"],\"cu\":\"%s\"}}",
      nodeId.c_str(), MQTT_TOPIC::DISINFECTION, MQTT_TOPIC::CMD_DISINFECTION, MQTT_TOPIC::STATE, nodeId.c_str(), configUrl.c_str());
    publishCfg("number", "disinfection", payload);
  }

  snprintf(payload, sizeof(payload),
    "{\"name\":\"OTA Status\",\"uniq_id\":\"%s_ota_status\",\"stat_t\":\"%s\","
    "\"icon\":\"mdi:update\","
    "\"avty_t\":\"%s\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\","
    "\"dev\":{\"ids\":[\"%s\"],\"cu\":\"%s\"}}",
    nodeId.c_str(), MQTT_TOPIC::OTA, MQTT_TOPIC::STATE, nodeId.c_str(), configUrl.c_str());
  publishCfg("sensor", "ota_status", payload);

  snprintf(payload, sizeof(payload),
    "{\"name\":\"URL OTA Update\",\"uniq_id\":\"%s_ota_update\",\"cmd_t\":\"%s\",\"pl_prs\":\"1\",\"ent_cat\":\"config\",\"icon\":\"mdi:update\","
    "\"avty_t\":\"%s\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\","
    "\"dev\":{\"ids\":[\"%s\"],\"cu\":\"%s\"}}",
    nodeId.c_str(), MQTT_TOPIC::CMD_OTA, MQTT_TOPIC::STATE, nodeId.c_str(), configUrl.c_str());
  publishCfg("button", "ota_update", payload);

  snprintf(payload, sizeof(payload),
    "{\"name\":\"Restart ESP\",\"uniq_id\":\"%s_restart\",\"cmd_t\":\"%s\",\"pl_prs\":\"1\",\"ent_cat\":\"config\",\"icon\":\"mdi:restart\","
    "\"avty_t\":\"%s\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\","
    "\"dev\":{\"ids\":[\"%s\"],\"cu\":\"%s\"}}",
    nodeId.c_str(), MQTT_TOPIC::CMD_RESTART, MQTT_TOPIC::STATE, nodeId.c_str(), configUrl.c_str());
  publishCfg("button", "restart", payload);

  snprintf(payload, sizeof(payload),
    "{\"name\":\"Republish Discovery\",\"uniq_id\":\"%s_republish\",\"cmd_t\":\"%s\",\"pl_prs\":\"1\",\"ent_cat\":\"config\",\"icon\":\"mdi:publish\","
    "\"avty_t\":\"%s\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\","
    "\"dev\":{\"ids\":[\"%s\"],\"cu\":\"%s\"}}",
    nodeId.c_str(), MQTT_TOPIC::CMD_REPUBLISH, MQTT_TOPIC::STATE, nodeId.c_str(), configUrl.c_str());
  publishCfg("button", "republish_discovery", payload);
}