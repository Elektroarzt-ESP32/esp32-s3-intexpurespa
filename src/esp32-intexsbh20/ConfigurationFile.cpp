/*
 * project:  Intex PureSpa WiFi Controller
 *
 * file:     ConfigurationFile.cpp
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

#include "ConfigurationFile.h"
#include "common.h"
#include <cstring>

void ConfigurationFile::copyToField(char* dst, size_t dstSize, const String& value)
{
  if (!dst || dstSize == 0) return;
  memset(dst, 0, dstSize);
  value.substring(0, dstSize - 1).toCharArray(dst, dstSize);
}

void ConfigurationFile::setDefaults()
{
  memset(&data, 0, sizeof(data));
  data.magic = MAGIC;
  data.version = VERSION;

  copyToField(data.wifiSSID, sizeof(data.wifiSSID), "");
  copyToField(data.wifiPassphrase, sizeof(data.wifiPassphrase), "");
  copyToField(data.firmwareURL, sizeof(data.firmwareURL), CONFIG::DEFAULT_FIRMWARE_URL);

  copyToField(data.mqttServer, sizeof(data.mqttServer), "");
  copyToField(data.mqttPort, sizeof(data.mqttPort), CONFIG::DEFAULT_MQTT_PORT);
  copyToField(data.mqttUser, sizeof(data.mqttUser), "");
  copyToField(data.mqttPassword, sizeof(data.mqttPassword), "");
  copyToField(data.mqttRetain, sizeof(data.mqttRetain), "no");
  copyToField(data.errorLanguage, sizeof(data.errorLanguage), "EN");
  copyToField(data.mqttDiscoveryMode, sizeof(data.mqttDiscoveryMode), CONFIG::DEFAULT_MQTT_DISCOVERY_MODE);
  copyToField(data.mqttDiscoveryPrefix, sizeof(data.mqttDiscoveryPrefix), CONFIG::DEFAULT_MQTT_DISCOVERY);
  copyToField(data.customModelName, sizeof(data.customModelName), CONFIG::DEFAULT_MODEL_NAME);
  copyToField(data.selectedModel, sizeof(data.selectedModel), CONFIG::DEFAULT_SELECTED_MODEL);
  copyToField(data.forceWifiSleep, sizeof(data.forceWifiSleep), CONFIG::DEFAULT_FORCE_WIFI_SLEEP);
}

bool ConfigurationFile::load(const char* fileName)
{
  (void)fileName;

  if (sizeof(ConfigData) > EEPROM_SIZE)
  {
    setDefaults();
    return false;
  }

  if (!EEPROM.begin(EEPROM_SIZE))
  {
    setDefaults();
    return false;
  }

  EEPROM.get(0, data);

  if (data.magic != MAGIC || data.version != VERSION)
  {
    setDefaults();
    save();
  }

  return true;
}

bool ConfigurationFile::exists(const char* tag) const
{
  return getField(tag) != nullptr;
}

const char* ConfigurationFile::getField(const char* tag) const
{
  if (strcmp(tag, CONFIG_TAG::WIFI_SSID) == 0) return data.wifiSSID;
  if (strcmp(tag, CONFIG_TAG::WIFI_PASSPHRASE) == 0) return data.wifiPassphrase;
  if (strcmp(tag, CONFIG_TAG::WIFI_OTA_URL) == 0) return data.firmwareURL;
  if (strcmp(tag, CONFIG_TAG::MQTT_SERVER) == 0) return data.mqttServer;
  if (strcmp(tag, CONFIG_TAG::MQTT_PORT) == 0) return data.mqttPort;
  if (strcmp(tag, CONFIG_TAG::MQTT_USER) == 0) return data.mqttUser;
  if (strcmp(tag, CONFIG_TAG::MQTT_PASSWORD) == 0) return data.mqttPassword;
  if (strcmp(tag, CONFIG_TAG::MQTT_RETAIN) == 0) return data.mqttRetain;
  if (strcmp(tag, CONFIG_TAG::MQTT_ERROR_LANG) == 0) return data.errorLanguage;
  if (strcmp(tag, CONFIG_TAG::MQTT_DISCOVERY_MODE) == 0) return data.mqttDiscoveryMode;
  if (strcmp(tag, CONFIG_TAG::MQTT_DISCOVERY_PREFIX) == 0) return data.mqttDiscoveryPrefix;
  if (strcmp(tag, CONFIG_TAG::CUSTOM_MODEL_NAME_KEY) == 0) return data.customModelName;
  if (strcmp(tag, CONFIG_TAG::SELECTED_MODEL_KEY) == 0) return data.selectedModel;
  if (strcmp(tag, CONFIG_TAG::FORCE_WIFI_SLEEP_KEY) == 0) return data.forceWifiSleep;
  return nullptr;
}

const char* ConfigurationFile::get(const char* tag) const
{
  const char* value = getField(tag);
  return value ? value : "";
}

bool ConfigurationFile::setField(const char* tag, const String& value)
{
  if (strcmp(tag, CONFIG_TAG::WIFI_SSID) == 0) { copyToField(data.wifiSSID, sizeof(data.wifiSSID), value); return true; }
  if (strcmp(tag, CONFIG_TAG::WIFI_PASSPHRASE) == 0) { copyToField(data.wifiPassphrase, sizeof(data.wifiPassphrase), value); return true; }
  if (strcmp(tag, CONFIG_TAG::WIFI_OTA_URL) == 0) { copyToField(data.firmwareURL, sizeof(data.firmwareURL), value); return true; }
  if (strcmp(tag, CONFIG_TAG::MQTT_SERVER) == 0) { copyToField(data.mqttServer, sizeof(data.mqttServer), value); return true; }
  if (strcmp(tag, CONFIG_TAG::MQTT_PORT) == 0) { copyToField(data.mqttPort, sizeof(data.mqttPort), value); return true; }
  if (strcmp(tag, CONFIG_TAG::MQTT_USER) == 0) { copyToField(data.mqttUser, sizeof(data.mqttUser), value); return true; }
  if (strcmp(tag, CONFIG_TAG::MQTT_PASSWORD) == 0) { copyToField(data.mqttPassword, sizeof(data.mqttPassword), value); return true; }
  if (strcmp(tag, CONFIG_TAG::MQTT_RETAIN) == 0) { copyToField(data.mqttRetain, sizeof(data.mqttRetain), value); return true; }
  if (strcmp(tag, CONFIG_TAG::MQTT_ERROR_LANG) == 0) { copyToField(data.errorLanguage, sizeof(data.errorLanguage), value); return true; }
  if (strcmp(tag, CONFIG_TAG::MQTT_DISCOVERY_MODE) == 0) { copyToField(data.mqttDiscoveryMode, sizeof(data.mqttDiscoveryMode), value); return true; }
  if (strcmp(tag, CONFIG_TAG::MQTT_DISCOVERY_PREFIX) == 0) { copyToField(data.mqttDiscoveryPrefix, sizeof(data.mqttDiscoveryPrefix), value); return true; }
  if (strcmp(tag, CONFIG_TAG::CUSTOM_MODEL_NAME_KEY) == 0) { copyToField(data.customModelName, sizeof(data.customModelName), value); return true; }
  if (strcmp(tag, CONFIG_TAG::SELECTED_MODEL_KEY) == 0) { copyToField(data.selectedModel, sizeof(data.selectedModel), value); return true; }
  if (strcmp(tag, CONFIG_TAG::FORCE_WIFI_SLEEP_KEY) == 0) { copyToField(data.forceWifiSleep, sizeof(data.forceWifiSleep), value); return true; }
  return false;
}

bool ConfigurationFile::set(const char* tag, const String& value)
{
  return setField(tag, value);
}

bool ConfigurationFile::save()
{
  data.magic = MAGIC;
  data.version = VERSION;
  EEPROM.put(0, data);
  return EEPROM.commit();
}