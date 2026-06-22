/*
 * project:  Intex PureSpa WiFi Controller
 *
 * file:     common.h
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

#ifndef COMMON_H
#define COMMON_H

#include <climits>
#include <stdint.h>
#include <WiFi.h>
#include <esp_rom_sys.h>

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int32_t  sint32;

static inline void feedWatchdog()
{
  yield();
}

static inline void wifiLightSleepOn()
{
}

static inline void wifiSleepOff()
{
}

static inline void wifiForceSleepBeginCompat()
{
}

static inline void wifiForceSleepWakeCompat()
{
}

/*****************************************************************************/
/*                           B U I L D   C O N F I G                         */
/*****************************************************************************/

// One firmware BIN supports both models.
// The active model is selected in the web UI and stored in flash memory.

#define DEFAULT_CUSTOM_MODEL_NAME_VALUE "Intex_PureSpa"

// #define FORCE_WIFI_SLEEP
#define SERIAL_DEBUG

/*****************************************************************************/

namespace CONFIG
{
  const char WIFI_VERSION[] = "1.4.3.16-esp32-s3";

  const unsigned long WIFI_MAX_DISCONNECT_DURATION = 900000;

  const unsigned int POOL_UPDATE_PERIOD         =   500;
  const unsigned int WIFI_UPDATE_PERIOD         = 30000;
  const unsigned int FORCED_STATE_UPDATE_PERIOD = 10000;

  const char DEFAULT_AP_NAME[]             = "Intex_PureSpa";
  const char DEFAULT_MODEL_NAME[]          = DEFAULT_CUSTOM_MODEL_NAME_VALUE;
  const char DEFAULT_SELECTED_MODEL[]     = "SB-H20";
  const char DEFAULT_MQTT_DISCOVERY[]      = "homeassistant";
  const char DEFAULT_MQTT_DISCOVERY_MODE[] = "HA";
  const char DEFAULT_MQTT_PORT[]           = "1883";
  const char DEFAULT_FIRMWARE_URL[]        = "http://192.168.0.20:8123/local/OTA_Update/esp32-intexsbh20.ino";
#ifdef FORCE_WIFI_SLEEP
  const char DEFAULT_FORCE_WIFI_SLEEP[]    = "yes";
#else
  const char DEFAULT_FORCE_WIFI_SLEEP[]    = "no";
#endif
}

namespace CONFIG_TAG
{
  const char WIFI_SSID[]             = "wifiSSID";
  const char WIFI_PASSPHRASE[]       = "wifiPassphrase";
  const char WIFI_OTA_URL[]          = "firmwareURL";

  const char MQTT_SERVER[]           = "mqttServer";
  const char MQTT_PORT[]             = "mqttPort";
  const char MQTT_USER[]             = "mqttUser";
  const char MQTT_PASSWORD[]         = "mqttPassword";
  const char MQTT_RETAIN[]           = "mqttRetain";
  const char MQTT_ERROR_LANG[]       = "errorLanguage";
  const char MQTT_DISCOVERY_MODE[]   = "mqttDiscoveryMode";
  const char MQTT_DISCOVERY_PREFIX[] = "mqttDiscoveryPrefix";

  // Active spa model (selected in the web UI; decoded in PureSpaIO at runtime)
  const char SELECTED_MODEL_KEY[] = "selectedModel";
  const char CUSTOM_MODEL_NAME_KEY[] = "customModelName";
  const char FORCE_WIFI_SLEEP_KEY[]  = "forceWifiSleep";
}

namespace MQTT_TOPIC
{
  const char BUBBLE[]       = "Intex_PureSpa/bubble";
  const char DISINFECTION[] = "Intex_PureSpa/disinfection";
  const char ERROR[]        = "Intex_PureSpa/error";
  const char FILTER[]       = "Intex_PureSpa/filter";
  const char HEATER[]       = "Intex_PureSpa/heater";
  const char JET[]          = "Intex_PureSpa/jet";
  // Used by HA MQTT climate entity
  const char CLIMATE_MODE[]   = "Intex_PureSpa/climate/mode";
  const char CLIMATE_ACTION[] = "Intex_PureSpa/climate/action";
  const char MODEL[]        = "Intex_PureSpa/model";
  const char POWER[]        = "Intex_PureSpa/power";
  const char WATER_ACT[]    = "Intex_PureSpa/water/tempAct";
  const char WATER_SET[]    = "Intex_PureSpa/water/tempSet";
  const char VERSION[]      = "Intex_PureSpa/wifi/version";
  const char IP[]           = "Intex_PureSpa/wifi/ip";
  const char RSSI[]         = "Intex_PureSpa/wifi/rssi";
  const char WIFI_TEMP[]    = "Intex_PureSpa/wifi/temp";
  const char STATE[]        = "Intex_PureSpa/wifi/state";
  const char OTA[]          = "Intex_PureSpa/wifi/update";

  const char CMD_BUBBLE[]       = "Intex_PureSpa/command/bubble";
  const char CMD_DISINFECTION[] = "Intex_PureSpa/command/disinfection";
  const char CMD_FILTER[]       = "Intex_PureSpa/command/filter";
  const char CMD_HEATER[]       = "Intex_PureSpa/command/heater";
  const char CMD_JET[]          = "Intex_PureSpa/command/jet";
  const char CMD_POWER[]        = "Intex_PureSpa/command/power";
  const char CMD_WATER[]        = "Intex_PureSpa/command/water/tempSet";
  const char CMD_OTA[]          = "Intex_PureSpa/wifi/command/update";
  const char CMD_RESTART[]      = "Intex_PureSpa/wifi/command/restart";
  const char CMD_REPUBLISH[]    = "Intex_PureSpa/wifi/command/republishDiscovery";
}

enum class LANG
{
  CODE = 0,
  EN = 1,
  DE = 2,
  CZ = 3
};

namespace PIN
{
  const uint8 CLOCK = 6;
  const uint8 DATA  = 7;
  const uint8 LATCH = 8;
  const uint8 NTC   = 4;
}

#ifdef SERIAL_DEBUG
#define DEBUG_MSG(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_MSG(...)
#endif

static inline unsigned long timeDiff(unsigned long newTime, unsigned long oldTime)
{
  if (newTime >= oldTime)
  {
    return newTime - oldTime;
  }
  else
  {
    return ULONG_MAX - oldTime + newTime + 1;
  }
}

static inline unsigned long diff(unsigned int newVal, unsigned int oldVal)
{
  if (newVal >= oldVal)
  {
    return newVal - oldVal;
  }
  else
  {
    return UINT_MAX - oldVal + newVal + 1;
  }
}

#endif