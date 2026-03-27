/*
 * project:  Intex PureSpa WiFi Controller
 *
 * file:     ConfigurationFile.h
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

#ifndef CONFIGURATION_FILE_H
#define CONFIGURATION_FILE_H

#include <Arduino.h>
#include <EEPROM.h>

class ConfigurationFile
{
public:
  bool load(const char* fileName = nullptr);
  bool exists(const char* tag) const;
  const char* get(const char* tag) const;
  bool set(const char* tag, const String& value);
  bool save();

private:
  static const uint32_t MAGIC = 0x49535041; // "ISPA"
  static const uint16_t VERSION = 2;
  static const size_t EEPROM_SIZE = 2048;

  struct ConfigData
  {
    uint32_t magic;
    uint16_t version;

    char wifiSSID[64];
    char wifiPassphrase[64];
    char firmwareURL[192];

    char mqttServer[64];
    char mqttPort[8];
    char mqttUser[64];
    char mqttPassword[64];
    char mqttRetain[8];
    char errorLanguage[8];
    char mqttDiscoveryMode[16];
    char mqttDiscoveryPrefix[64];

    char customModelName[64];
    char selectedModel[16];
    char forceWifiSleep[8];
  };

private:
  ConfigData data{};

  void setDefaults();
  const char* getField(const char* tag) const;
  bool setField(const char* tag, const String& value);
  static void copyToField(char* dst, size_t dstSize, const String& value);
};

#endif