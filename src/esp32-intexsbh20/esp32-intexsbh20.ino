/*
 * project:  Intex PureSpa WiFi Controller
 *
 * file:     esp32-intexsbh20.ino
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
 * Hardware:
 * ESP32 firmware; SB-H20 and SJB-HS panel models.
 * Target: ESP32 (Arduino-ESP32; GPIO numbers refer to the chip, not the silkscreen).
 * GPIO pins (see common.h, namespace PIN):
 *   18  CLOCK  — spa panel bus clock (digital input, interrupt on rising edge)
 *   19  DATA   — spa panel bus data line (input; open-drain when driving)
 *   23  LATCH  — spa panel bus latch / frame sync (digital input)
 *   34  NTC    — ADC1, on-board NTC thermistor for controller temperature
 */

#include "common.h"
#include "ConfigurationFile.h"
#include "MQTTClient.h"
#include "MQTTPublisher.h"
#include "NTCThermometer.h"
#include "OTAUpdate.h"
#include "PureSpaIO.h"
#include "WebConfig.h"
#include <esp_system.h>
#include <stdexcept>

ConfigurationFile config;
NTCThermometer thermometer;
OTAUpdate otaUpdate;
PureSpaIO pureSpaIO;
WebConfig webConfig;

MQTTClient mqttClient;
MQTTPublisher mqttPublisher(mqttClient, pureSpaIO, thermometer);
String username;
String password;
String mqttDeviceName;
String discoveryPrefix;

unsigned long disconnectTime = 0;

LANG language = LANG::CODE;
bool initialized = false;
bool otaStatusInitialized = false;
static bool prevMqttConnected = false;

// Debounce for temperature commands (avoid repeated setpoint changes).
static const unsigned long WATER_SET_DEBOUNCE_MS = 1500;
int pendingWaterSetTempC = PureSpaIO::UNDEF::INT;
unsigned long pendingWaterSetTempAtMs = 0;

// Boot-only one-shot sync of tempSet from panel UI.
enum BootTempSetSyncState
{
  BOOT_TEMPSET_IDLE = 0,
  BOOT_TEMPSET_WAIT_POWER_ON,
  BOOT_TEMPSET_WAIT_READ,
  BOOT_TEMPSET_WAIT_POWER_OFF,
  BOOT_TEMPSET_DONE
};

BootTempSetSyncState bootTempSetSyncState = BOOT_TEMPSET_IDLE;
bool bootTempSetRestorePowerOff = false;
unsigned long bootTempSetStepAtMs = 0;
uint8_t bootTempSetPowerOffRetries = 0;
unsigned long bootSpaOnlineSinceMs = 0;
static const unsigned long BOOT_TEMPSET_START_AFTER_ONLINE_MS = 1200;
bool bootSwitchSnapshotPublished = false;

// After manual/power-on: one arrow click if tempSet still unknown (boot discovery already finished).
static const unsigned long POWER_ON_TEMP_PROBE_DELAY_MS = 800;
static bool prevSpaPowerOnSample = false;
static bool prevSpaPowerOnValid = false;
static unsigned long powerOnTempProbeScheduledAtMs = 0;

static String readConfigOrDefault(const char* key, const char* fallback = "")
{
  if (config.exists(key))
  {
    return String(config.get(key));
  }
  return String(fallback ? fallback : "");
}

void setup()
{
  Serial.begin(74880);
  Serial.setTxTimeoutMs(0); // non-blocking USB CDC on ESP32-S3
  delay(20);

  const esp_reset_reason_t rr = esp_reset_reason();
  Serial.printf("ESP reset reason %d (9=brownout)\n", static_cast<int>(rr));
  if (rr == ESP_RST_BROWNOUT)
  {
    delay(500);
  }

  config.load();

  Serial.printf("%s MQTT WiFi Controller %s\n", pureSpaIO.getModelName(), CONFIG::WIFI_VERSION);
  Serial.printf("build with Arduino Core for ESP32 %s\n", ESP.getSdkVersion());
  Serial.printf("PIN CLOCK=%u DATA=%u LATCH=%u NTC=%u\n",
                PIN::CLOCK, PIN::DATA, PIN::LATCH, PIN::NTC);
  Serial.printf("Free heap: %u bytes\n", static_cast<unsigned int>(ESP.getFreeHeap()));

  webConfig.begin();

  bool ready = false;
  if (config.load())
  {
    try
    {
      // WiFi.mode() already set by webConfig.begin() above.
      // A second call resets the running SoftAP on ESP32-S3/Arduino Core v5.x.
      WiFi.begin(config.get(CONFIG_TAG::WIFI_SSID), config.get(CONFIG_TAG::WIFI_PASSPHRASE));

      bool retainAll = readConfigOrDefault(CONFIG_TAG::MQTT_RETAIN, "no") != "no";
      mqttPublisher.setRetainAll(retainAll);

      mqttDeviceName = readConfigOrDefault(CONFIG_TAG::CUSTOM_MODEL_NAME_KEY, pureSpaIO.getModelName());
      if (!mqttDeviceName.length())
      {
        mqttDeviceName = pureSpaIO.getModelName();
      }

      discoveryPrefix = readConfigOrDefault(CONFIG_TAG::MQTT_DISCOVERY_PREFIX, CONFIG::DEFAULT_MQTT_DISCOVERY);
      String discoveryMode = readConfigOrDefault(CONFIG_TAG::MQTT_DISCOVERY_MODE, CONFIG::DEFAULT_MQTT_DISCOVERY_MODE);
      bool haDiscovery = (discoveryMode != "DIRECT");
      mqttClient.configureDiscovery(discoveryPrefix.c_str(), haDiscovery);

      String forceSleep = readConfigOrDefault(CONFIG_TAG::FORCE_WIFI_SLEEP_KEY, "no");
      pureSpaIO.setForceWifiSleep(forceSleep != "no");

      String selectedModel = readConfigOrDefault(CONFIG_TAG::SELECTED_MODEL_KEY, CONFIG::DEFAULT_SELECTED_MODEL);
      pureSpaIO.setModelFromString(selectedModel.c_str());

      mqttClient.addMetadata(
        MQTT_TOPIC::MODEL,
        pureSpaIO.getModel() == PureSpaIO::MODEL::SJBHS ? "SJB-HS" : "SB-H20 / SSP-H-20-1 / SB-B20"
      );
      mqttClient.addMetadata(MQTT_TOPIC::VERSION, CONFIG::WIFI_VERSION);
      mqttClient.addMetadata(MQTT_TOPIC::IP, WiFi.localIP().toString().c_str());

      mqttClient.addSubscriber(MQTT_TOPIC::CMD_BUBBLE, [](bool b) -> void {
        pureSpaIO.setBubbleOn(b);
      });

      mqttClient.addSubscriber(MQTT_TOPIC::CMD_FILTER, [](bool b) -> void {
        pureSpaIO.setFilterOn(b);
      });

      mqttClient.addSubscriber(MQTT_TOPIC::CMD_HEATER, [](bool b) -> void {
        pureSpaIO.markTempDisplayDisturbance();
        if (b)
        {
          // HA climate "heat" should turn the whole system on (if needed)
          // and then enable heating.
          if (!pureSpaIO.isPowerOn())
          {
            pureSpaIO.setPowerOn(true);

            // Wait a short moment until the power state updates.
            unsigned long start = millis();
            while (!pureSpaIO.isPowerOn() && timeDiff(millis(), start) < 3000)
            {
              delay(100);
              yield();
            }
          }

          pureSpaIO.setHeaterOn(true);
        }
        else
        {
          // HA climate "off" should disable heating.
          pureSpaIO.setHeaterOn(false);
        }
      });

      mqttClient.addSubscriber(MQTT_TOPIC::CMD_POWER, [](bool b) -> void {
        pureSpaIO.markTempDisplayDisturbance();
        pureSpaIO.setPowerOn(b);
        delay(200);
        yield();

        mqttClient.publish(MQTT_TOPIC::POWER, pureSpaIO.isPowerOn() ? "on" : "off", true, true);
      });

      mqttClient.addSubscriber(MQTT_TOPIC::CMD_WATER, [](int i) -> void {
        if (i < PureSpaIO::WATER_TEMP::SET_MIN || i > PureSpaIO::WATER_TEMP::SET_MAX)
        {
          return;
        }

        pureSpaIO.markTempDisplayDisturbance();
        pureSpaIO.setAuthoritativeDesiredCelsius(i);

        // Debounce: only apply after there was no new command for a moment.
        pendingWaterSetTempC = i;
        pendingWaterSetTempAtMs = millis();

        // Stop the startup "sync displayed setpoint" logic while the user is controlling the setpoint.
      });

      if (pureSpaIO.getModel() == PureSpaIO::MODEL::SJBHS)
      {
        mqttClient.addSubscriber(MQTT_TOPIC::CMD_DISINFECTION, [](int i) -> void {
          pureSpaIO.setDisinfectionTime(i);
        });

        mqttClient.addSubscriber(MQTT_TOPIC::CMD_JET, [](bool b) -> void {
          pureSpaIO.setJetOn(b);
        });

        mqttClient.addMetadata(MQTT_TOPIC::DISINFECTION, "enabled");
        mqttClient.addMetadata(MQTT_TOPIC::JET, "enabled");
      }

      if (config.exists(CONFIG_TAG::MQTT_ERROR_LANG))
      {
        String lang = config.get(CONFIG_TAG::MQTT_ERROR_LANG);
        if (lang == "EN")
        {
          language = LANG::EN;
        }
        else if (lang == "DE")
        {
          language = LANG::DE;
        }
        else if (lang == "CZ")
        {
          language = LANG::CZ;
        }
        else
        {
          language = LANG::CODE;
        }
      }

      if (config.exists(CONFIG_TAG::MQTT_USER))
      {
        username = config.get(CONFIG_TAG::MQTT_USER);
        password = config.get(CONFIG_TAG::MQTT_PASSWORD);
      }

      uint16 mqttPort = config.exists(CONFIG_TAG::MQTT_PORT) ? atoi(config.get(CONFIG_TAG::MQTT_PORT)) : 1883;

      mqttClient.setup(
        config.get(CONFIG_TAG::MQTT_SERVER),
        mqttPort,
        username.c_str(),
        password.c_str(),
        mqttDeviceName.c_str(),
        MQTT_TOPIC::STATE,
        "offline"
      );

      thermometer.setup(22000, 3.30f, 1.0f);
      ready = true;
    }
    catch (const std::runtime_error& re)
    {
      Serial.println(re.what());
    }
  }

  if (!ready)
  {
    Serial.println("Config not ready, web config is available.");
    return;
  }
}

void loop()
{
  webConfig.loop();
  feedWatchdog();

  wl_status_t wifiStatus = WiFi.status();
  unsigned long now = millis();

  if (wifiStatus == WL_CONNECTED)
  {
    disconnectTime = 0;

    if (!initialized)
    {
      mqttClient.addMetadata(MQTT_TOPIC::IP, WiFi.localIP().toString().c_str());
      pureSpaIO.setup(language);

      initialized = true;
      otaStatusInitialized = false;
      bootTempSetSyncState = BOOT_TEMPSET_IDLE;
      bootTempSetRestorePowerOff = false;
      bootTempSetStepAtMs = 0;
      bootTempSetPowerOffRetries = 0;
      bootSpaOnlineSinceMs = 0;
      bootSwitchSnapshotPublished = false;
      prevSpaPowerOnValid = false;
      prevSpaPowerOnSample = false;
      powerOnTempProbeScheduledAtMs = 0;
    }
    else
    {
      mqttClient.loop();

      if (mqttClient.isConnected() && !prevMqttConnected)
      {
        mqttPublisher.publishWifiDiagnosticsNow();
      }
      prevMqttConnected = mqttClient.isConnected();

      if (mqttClient.isConnected() && !otaStatusInitialized)
      {
        // Make OTA status immediately available in HA after first connect/restart.
        mqttClient.publish(MQTT_TOPIC::OTA, "idle", true, true);
        otaStatusInitialized = true;
      }
      else if (!mqttClient.isConnected())
      {
        otaStatusInitialized = false;
        bootTempSetSyncState = BOOT_TEMPSET_IDLE;
        bootTempSetRestorePowerOff = false;
        bootTempSetStepAtMs = 0;
        bootTempSetPowerOffRetries = 0;
        bootSwitchSnapshotPublished = false;
        prevSpaPowerOnValid = false;
        prevSpaPowerOnSample = false;
        powerOnTempProbeScheduledAtMs = 0;
      }

      if (pureSpaIO.isOnline())
      {
        if (bootSpaOnlineSinceMs == 0)
        {
          bootSpaOnlineSinceMs = now;
        }
      }
      else
      {
        bootSpaOnlineSinceMs = 0;
      }

      pureSpaIO.loop();
      mqttPublisher.loop();

      // One-time forced switch snapshot for HA right after online/connect.
      if (mqttClient.isConnected() && pureSpaIO.isOnline() && !bootSwitchSnapshotPublished)
      {
        const uint8 bubbleState = pureSpaIO.isBubbleOn();
        const uint8 filterState = pureSpaIO.isFilterOn();
        const uint8 powerState = pureSpaIO.isPowerOn();
        const uint8 heaterState = pureSpaIO.isHeaterOn();

        if (bubbleState != PureSpaIO::UNDEF::BOOL
            && filterState != PureSpaIO::UNDEF::BOOL
            && powerState != PureSpaIO::UNDEF::BOOL)
        {
          const bool sjbhsReady =
            pureSpaIO.getModel() != PureSpaIO::MODEL::SJBHS
            || (pureSpaIO.isJetOn() != PureSpaIO::UNDEF::BOOL
                && pureSpaIO.getDisinfectionTime() != PureSpaIO::UNDEF::INT);

          if (sjbhsReady)
          {
            mqttClient.publish(MQTT_TOPIC::BUBBLE, bubbleState ? "on" : "off", true, true);
            mqttClient.publish(MQTT_TOPIC::FILTER, filterState ? "on" : "off", true, true);
            mqttClient.publish(MQTT_TOPIC::POWER, powerState ? "on" : "off", true, true);

            if (heaterState != PureSpaIO::UNDEF::BOOL)
            {
              mqttClient.publish(
                MQTT_TOPIC::HEATER,
                heaterState ? (pureSpaIO.isHeaterStandby() ? "standby" : "on") : "off",
                true,
                true
              );
            }

            if (pureSpaIO.getModel() == PureSpaIO::MODEL::SJBHS)
            {
              mqttClient.publish(
                MQTT_TOPIC::JET,
                pureSpaIO.isJetOn() ? "on" : "off",
                true,
                true
              );
              mqttClient.publish(
                MQTT_TOPIC::DISINFECTION,
                String(pureSpaIO.getDisinfectionTime()),
                true,
                true
              );
            }

            bootSwitchSnapshotPublished = true;
          }
        }
      }

      // Apply debounced water temperature setpoint
      if (pendingWaterSetTempC != PureSpaIO::UNDEF::INT
          && pendingWaterSetTempAtMs != 0
          && timeDiff(now, pendingWaterSetTempAtMs) >= WATER_SET_DEBOUNCE_MS)
      {
        // Only attempt when power state is explicitly ON.
        // Keep pending command while OFF/unknown so it is not lost.
        const uint8 powerState = pureSpaIO.isPowerOn();
        if (powerState == true)
        {
          int target = pendingWaterSetTempC;
          pureSpaIO.setDesiredWaterTempCelsius(target);
          delay(300);
          yield();

          int setTemp = pureSpaIO.getDesiredWaterTempCelsius();
          if (setTemp != PureSpaIO::UNDEF::INT)
          {
            pendingWaterSetTempC = PureSpaIO::UNDEF::INT;
            pendingWaterSetTempAtMs = 0;

            mqttClient.publish(MQTT_TOPIC::WATER_SET, String(setTemp), true, true);
            pureSpaIO.setAuthoritativeDesiredCelsius(setTemp);
            // Re-apply water suppression after button clicks settle.
            pureSpaIO.markTempDisplayDisturbance();
          }
          else
          {
            // Retry later if setpoint is not yet readable.
            pendingWaterSetTempAtMs = now;
          }
        }
      }

      // Boot-only tempSet discovery:
      // If HA does not know the setpoint yet, perform one safe sequence:
      // power on (if needed) -> request setpoint display -> read -> restore power state.
      const bool bootTempSetCanStart =
        mqttClient.isConnected()
        && pureSpaIO.isOnline()
        && bootSpaOnlineSinceMs != 0
        && timeDiff(now, bootSpaOnlineSinceMs) >= BOOT_TEMPSET_START_AFTER_ONLINE_MS
        && pendingWaterSetTempC == PureSpaIO::UNDEF::INT;

      const bool bootTempSetInProgress =
        bootTempSetSyncState != BOOT_TEMPSET_IDLE
        && bootTempSetSyncState != BOOT_TEMPSET_DONE;

      if (bootTempSetCanStart || bootTempSetInProgress)
      {
        // Only skip whole boot discovery when idle and tempSet already known (e.g. retained MQTT).
        // Do NOT jump to DONE mid-sequence — that would abort before power-off / panel read.
        if (bootTempSetSyncState == BOOT_TEMPSET_IDLE
            && pureSpaIO.getAuthoritativeDesiredWaterTempCelsius() != PureSpaIO::UNDEF::INT)
        {
          bootTempSetSyncState = BOOT_TEMPSET_DONE;
        }
        else if (bootTempSetSyncState != BOOT_TEMPSET_DONE)
        {
          switch (bootTempSetSyncState)
          {
            case BOOT_TEMPSET_IDLE:
            {
              const uint8 powerState = pureSpaIO.isPowerOn();
              if (powerState == PureSpaIO::UNDEF::BOOL)
              {
                break;
              }

              if (powerState == false)
              {
                bootTempSetRestorePowerOff = true;
                pureSpaIO.setPowerOn(true);
              }
              else
              {
                bootTempSetRestorePowerOff = false;
              }

              bootTempSetStepAtMs = now;
              bootTempSetSyncState = BOOT_TEMPSET_WAIT_POWER_ON;
              break;
            }

            case BOOT_TEMPSET_WAIT_POWER_ON:
            {
              if (timeDiff(now, bootTempSetStepAtMs) < 1000)
              {
                break;
              }

              if (pureSpaIO.isPowerOn() != true)
              {
                if (bootTempSetRestorePowerOff)
                {
                  pureSpaIO.setPowerOn(false);
                }
                bootTempSetSyncState = BOOT_TEMPSET_DONE;
                break;
              }

              if (!pureSpaIO.requestSetTempDisplay())
              {
                if (bootTempSetRestorePowerOff)
                {
                  pureSpaIO.setPowerOn(false);
                }
                bootTempSetSyncState = BOOT_TEMPSET_DONE;
                break;
              }

              bootTempSetStepAtMs = now;
              bootTempSetSyncState = BOOT_TEMPSET_WAIT_READ;
              break;
            }

            case BOOT_TEMPSET_WAIT_READ:
            {
              int setTemp = pureSpaIO.getDesiredWaterTempCelsius();
              if (setTemp != PureSpaIO::UNDEF::INT)
              {
                pureSpaIO.setAuthoritativeDesiredCelsius(setTemp);
                mqttClient.publish(MQTT_TOPIC::WATER_SET, String(setTemp), true, true);

                if (bootTempSetRestorePowerOff)
                {
                  pureSpaIO.setPowerOn(false);
                  mqttClient.publish(MQTT_TOPIC::POWER, "off", true, true);
                  bootTempSetPowerOffRetries = 0;
                  bootTempSetStepAtMs = now;
                  bootTempSetSyncState = BOOT_TEMPSET_WAIT_POWER_OFF;
                  break;
                }

                bootTempSetSyncState = BOOT_TEMPSET_DONE;
                break;
              }

              // Read window after entering setpoint UI; then finish gracefully.
              if (timeDiff(now, bootTempSetStepAtMs) >= 1500)
              {
                if (bootTempSetRestorePowerOff)
                {
                  pureSpaIO.setPowerOn(false);
                  mqttClient.publish(MQTT_TOPIC::POWER, "off", true, true);
                  bootTempSetPowerOffRetries = 0;
                  bootTempSetStepAtMs = now;
                  bootTempSetSyncState = BOOT_TEMPSET_WAIT_POWER_OFF;
                  break;
                }
                bootTempSetSyncState = BOOT_TEMPSET_DONE;
              }
              break;
            }

            case BOOT_TEMPSET_WAIT_POWER_OFF:
            {
              if (pureSpaIO.isPowerOn() == false)
              {
                mqttClient.publish(MQTT_TOPIC::POWER, "off", true, true);
                bootTempSetSyncState = BOOT_TEMPSET_DONE;
                break;
              }

              if (timeDiff(now, bootTempSetStepAtMs) >= 1200)
              {
                if (bootTempSetPowerOffRetries < 12)
                {
                  pureSpaIO.setPowerOn(false);
                  mqttClient.publish(MQTT_TOPIC::POWER, "off", true, true);
                  bootTempSetPowerOffRetries++;
                  bootTempSetStepAtMs = now;
                }
                else
                {
                  // Hard fallback: keep trying until panel actually powers off.
                  pureSpaIO.setPowerOn(false);
                  mqttClient.publish(MQTT_TOPIC::POWER, "off", true, true);
                  bootTempSetStepAtMs = now;
                }
              }
              break;
            }

            case BOOT_TEMPSET_DONE:
            default:
              break;
          }
        }
      }

      // Power-on probe: after boot temp discovery is finished, if user turns spa ON and tempSet
      // is still unknown, do a single arrow press to show setpoint (no full boot sequence).
      {
        const uint8 pwrEdge = pureSpaIO.isPowerOn();
        if (pwrEdge == PureSpaIO::UNDEF::BOOL)
        {
          // keep prev until power state is known
        }
        else if (pwrEdge == false)
        {
          prevSpaPowerOnSample = false;
          prevSpaPowerOnValid = true;
          powerOnTempProbeScheduledAtMs = 0;
        }
        else
        {
          const bool rising =
            prevSpaPowerOnValid && !prevSpaPowerOnSample;
          if (rising
              && mqttClient.isConnected()
              && pureSpaIO.isOnline()
              && bootTempSetSyncState == BOOT_TEMPSET_DONE
              && pureSpaIO.getAuthoritativeDesiredWaterTempCelsius() == PureSpaIO::UNDEF::INT
              && pendingWaterSetTempC == PureSpaIO::UNDEF::INT)
          {
            powerOnTempProbeScheduledAtMs = now;
          }
          prevSpaPowerOnSample = true;
          prevSpaPowerOnValid = true;
        }
      }

      if (powerOnTempProbeScheduledAtMs != 0
          && timeDiff(now, powerOnTempProbeScheduledAtMs) >= POWER_ON_TEMP_PROBE_DELAY_MS)
      {
        powerOnTempProbeScheduledAtMs = 0;
        if (mqttClient.isConnected()
            && pureSpaIO.isOnline()
            && pureSpaIO.isPowerOn() == true
            && pureSpaIO.getAuthoritativeDesiredWaterTempCelsius() == PureSpaIO::UNDEF::INT
            && pendingWaterSetTempC == PureSpaIO::UNDEF::INT
            && bootTempSetSyncState == BOOT_TEMPSET_DONE)
        {
          pureSpaIO.markTempDisplayDisturbance();
          if (pureSpaIO.requestSetTempDisplay())
          {
            delay(300);
            yield();
            const int t = pureSpaIO.getDesiredWaterTempCelsius();
            if (t != PureSpaIO::UNDEF::INT)
            {
              pureSpaIO.setAuthoritativeDesiredCelsius(t);
              mqttClient.publish(MQTT_TOPIC::WATER_SET, String(t), true, true);
            }
          }
        }
      }

      delay(20);
    }
  }
  else
  {
    prevMqttConnected = false;
    if (!disconnectTime)
    {
      disconnectTime = now;
    }
    else if (timeDiff(now, disconnectTime) > CONFIG::WIFI_MAX_DISCONNECT_DURATION)
    {
      DEBUG_MSG("restarting ... (no WiFi connection for several minutes)\n");
      ESP.restart();
    }
  }
}