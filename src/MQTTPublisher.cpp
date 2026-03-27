/*
 * project:  Intex PureSpa WiFi Controller
 *
 * file:     MQTTPublisher.cpp
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

#include "MQTTPublisher.h"

#include "MQTTClient.h"
#include "PureSpaIO.h"
#include "NTCThermometer.h"
#include "common.h"

namespace
{
  static int g_lastPublishedWaterTemp = PureSpaIO::UNDEF::INT;
  static unsigned long g_lastPublishedWaterTempTime = 0;
  static int g_pendingWaterTemp = PureSpaIO::UNDEF::INT;
  static unsigned long g_pendingWaterTempTime = 0;
  static bool g_wasSpaOnline = false;
}


MQTTPublisher::MQTTPublisher(MQTTClient& mqttClient, PureSpaIO& pureSpaIO, NTCThermometer& thermometer) :
  mqttClient(mqttClient),
  pureSpaIO(pureSpaIO),
  thermometer(thermometer),
  retainAll(false)
{
}

/**
 * set retain flag in all published MQTT messages
 */
void MQTTPublisher::setRetainAll(bool retain)
{
  retainAll = retain;
}

bool MQTTPublisher::isRetainAll() const
{
  return retainAll;
}

bool MQTTPublisher::hasInitialSwitchStatesPublished() const
{
  return initialSwitchStatesPublished;
}

bool MQTTPublisher::hasInitialCoreSwitchStatesPublished() const
{
  return initialBubblePublished
      && initialFilterPublished
      && initialPowerPublished;
}

bool MQTTPublisher::hasInitialClimateStatesPublished() const
{
  return initialClimateStatesPublished;
}

void MQTTPublisher::publishIfDefined(const char* topic, uint8 b, uint8 undef)
{
  if (b != undef)
  {
    mqttClient.publish(topic, b? "on" : "off", retainAll);
  }
}

void MQTTPublisher::publishIfDefined(const char* topic, int i, int undef)
{
  if (i != undef)
  {
     publish(topic, i);
  }
}

void MQTTPublisher::publishIfDefined(const char* topic, uint16 u, uint16 undef)
{
  if (u != undef)
  {
     publish(topic, u);
  }
}

void MQTTPublisher::publish(const char* topic, int i)
{
  snprintf(buf, BUFFER_SIZE, "%d", i);
  mqttClient.publish(topic, buf, retainAll);
}

void MQTTPublisher::publish(const char* topic, unsigned int u)
{
  snprintf(buf, BUFFER_SIZE, "%u", u);
  mqttClient.publish(topic, buf, retainAll);
}

void MQTTPublisher::publishTemp(const char* topic, float t, bool forcePublish)
{
  snprintf(buf, BUFFER_SIZE, "%.0f", t);
  if (t >= -60 && t <= 145)
  {
    DEBUG_MSG("controller temperature: %s °C\n", buf);
    mqttClient.publish(topic, String(buf), retainAll, forcePublish);
  }
  else
  {
    DEBUG_MSG("controller temperature: error (%s °C)\n", buf);
    mqttClient.publish(topic, String("error"), retainAll, forcePublish);
  }
}

void MQTTPublisher::publishWifiDiagnostics(unsigned long now)
{
  wifiStateUpdateTime = now;

  publishTemp(MQTT_TOPIC::WIFI_TEMP, thermometer.getTemperature(), true);

  mqttClient.publish(MQTT_TOPIC::IP, WiFi.localIP().toString(), true, true);

  snprintf(buf, BUFFER_SIZE, "%d", WiFi.RSSI());
  mqttClient.publish(MQTT_TOPIC::RSSI, String(buf), true, true);

#ifdef SERIAL_DEBUG
  publish("wifi/heap", ESP.getFreeHeap());
#endif
}

void MQTTPublisher::publishWifiDiagnosticsNow()
{
  if (!mqttClient.isConnected())
  {
    return;
  }
  publishWifiDiagnostics(millis());
}

/**
 * publish changed topics with rate limit
 * except topic 'wifi/state' that is force published ever 10 seconds
 */
void MQTTPublisher::loop()
{
  unsigned long now = millis();
  if (timeDiff(now, poolUpdateTime) >= CONFIG::POOL_UPDATE_PERIOD)
  {
    poolUpdateTime = now;

    bool forcedStateUpdate = false;
    if (timeDiff(now, poolStateUpdateTime) >= CONFIG::FORCED_STATE_UPDATE_PERIOD)
    {
      poolStateUpdateTime = now;
      forcedStateUpdate = true;
    }

    if (pureSpaIO.isOnline())
    {
      if (!g_wasSpaOnline)
      {
        g_wasSpaOnline = true;
        g_lastPublishedWaterTemp = PureSpaIO::UNDEF::INT;
        g_pendingWaterTemp = PureSpaIO::UNDEF::INT;
        g_pendingWaterTempTime = 0;
        initialSwitchStatesPublished = false;
        initialBubblePublished = false;
        initialFilterPublished = false;
        initialPowerPublished = false;
        initialHeaterPublished = false;
        initialJetPublished = false;
        initialDisinfectionPublished = false;
        initialClimateStatesPublished = false;
      }

      const uint8 bubbleState = pureSpaIO.isBubbleOn();
      const uint8 filterState = pureSpaIO.isFilterOn();
      const uint8 powerState = pureSpaIO.isPowerOn();
      const uint8 heaterState = pureSpaIO.isHeaterOn();

      if (!initialBubblePublished && bubbleState != PureSpaIO::UNDEF::BOOL)
      {
        mqttClient.publish(MQTT_TOPIC::BUBBLE, bubbleState ? "on" : "off", true, true);
        initialBubblePublished = true;
      }
      if (!initialFilterPublished && filterState != PureSpaIO::UNDEF::BOOL)
      {
        mqttClient.publish(MQTT_TOPIC::FILTER, filterState ? "on" : "off", true, true);
        initialFilterPublished = true;
      }
      if (!initialPowerPublished && powerState != PureSpaIO::UNDEF::BOOL)
      {
        mqttClient.publish(MQTT_TOPIC::POWER, powerState ? "on" : "off", true, true);
        initialPowerPublished = true;
      }
      if (!initialHeaterPublished && heaterState != PureSpaIO::UNDEF::BOOL)
      {
        mqttClient.publish(
          MQTT_TOPIC::HEATER,
          heaterState ? (pureSpaIO.isHeaterStandby() ? "standby" : "on") : "off",
          true,
          true
        );
        initialHeaterPublished = true;
      }
      if (pureSpaIO.getModel() == PureSpaIO::MODEL::SJBHS)
      {
        if (!initialJetPublished && pureSpaIO.isJetOn() != PureSpaIO::UNDEF::BOOL)
        {
          mqttClient.publish(
            MQTT_TOPIC::JET,
            pureSpaIO.isJetOn() ? "on" : "off",
            true,
            true
          );
          initialJetPublished = true;
        }
        if (!initialDisinfectionPublished
            && pureSpaIO.getDisinfectionTime() != PureSpaIO::UNDEF::INT)
        {
          snprintf(
            buf,
            BUFFER_SIZE,
            "%d",
            pureSpaIO.getDisinfectionTime()
          );
          mqttClient.publish(MQTT_TOPIC::DISINFECTION, String(buf), true, true);
          initialDisinfectionPublished = true;
        }
      }
      if (!initialSwitchStatesPublished
          && initialBubblePublished
          && initialFilterPublished
          && initialPowerPublished
          && initialHeaterPublished
          && (pureSpaIO.getModel() != PureSpaIO::MODEL::SJBHS
              || (initialJetPublished && initialDisinfectionPublished)))
      {
        initialSwitchStatesPublished = true;
      }

      publishIfDefined(MQTT_TOPIC::BUBBLE, bubbleState, PureSpaIO::UNDEF::BOOL);
      publishIfDefined(MQTT_TOPIC::FILTER, filterState, PureSpaIO::UNDEF::BOOL);
      publishIfDefined(MQTT_TOPIC::POWER, powerState, PureSpaIO::UNDEF::BOOL);

      if (pureSpaIO.getModel() == PureSpaIO::MODEL::SJBHS)
      {
        publishIfDefined(MQTT_TOPIC::DISINFECTION, pureSpaIO.getDisinfectionTime(), (int)PureSpaIO::UNDEF::USHORT);
        publishIfDefined(MQTT_TOPIC::JET, pureSpaIO.isJetOn(), PureSpaIO::UNDEF::BOOL);
      }

      uint8 b = heaterState;
      if (b != PureSpaIO::UNDEF::BOOL)
      {
        mqttClient.publish(MQTT_TOPIC::HEATER, b? (pureSpaIO.isHeaterStandby()? "standby" : "on") : "off", retainAll);
      }

      // HA climate state topics (mode/action) so HA knows the real heating state.
      // Modes: "off" / "heat"
      // Actions: "idle" / "heating"
      const bool heaterOnOrStandby = pureSpaIO.isHeaterOn();
      const bool heaterIsStandby = pureSpaIO.isHeaterStandby();

      mqttClient.publish(
        MQTT_TOPIC::CLIMATE_MODE,
        heaterOnOrStandby ? "heat" : "off",
        retainAll
      );
      mqttClient.publish(
        MQTT_TOPIC::CLIMATE_ACTION,
        heaterOnOrStandby
          ? (heaterIsStandby ? "idle" : "heating")
          : "off",
        retainAll
      );

      // Publish WATER_SET only from authoritative setpoint (MQTT / confirmed change), not decoded display.
      const int desiredWaterC = pureSpaIO.getAuthoritativeDesiredWaterTempCelsius();

      int waterTemp = pureSpaIO.getActWaterTempCelsius();
      if (waterTemp != PureSpaIO::UNDEF::INT)
      {
        bool allowPublish = false;

        if (g_lastPublishedWaterTemp == PureSpaIO::UNDEF::INT)
        {
          allowPublish = true;
        }
        else
        {
          int diffTemp = abs(waterTemp - g_lastPublishedWaterTemp);
          unsigned long age = timeDiff(now, g_lastPublishedWaterTempTime);

          if (diffTemp <= 1)
          {
            allowPublish = true;
          }
          else if (age >= 45000)
          {
            allowPublish = true;
          }
          else
          {
            if (g_pendingWaterTemp == waterTemp && timeDiff(now, g_pendingWaterTempTime) >= 3000)
            {
              allowPublish = true;
            }
            else
            {
              g_pendingWaterTemp = waterTemp;
              g_pendingWaterTempTime = now;
            }
          }
        }

        if (allowPublish)
        {
          snprintf(buf, BUFFER_SIZE, "%d", waterTemp);
          String payload(buf);
          mqttClient.publish(MQTT_TOPIC::WATER_ACT, payload, retainAll, true);
          g_lastPublishedWaterTemp = waterTemp;
          g_lastPublishedWaterTempTime = now;
          g_pendingWaterTemp = PureSpaIO::UNDEF::INT;
          g_pendingWaterTempTime = 0;
        }
      }
      {
        if (desiredWaterC != PureSpaIO::UNDEF::INT)
        {
          snprintf(buf, BUFFER_SIZE, "%d", desiredWaterC);
          mqttClient.publish(MQTT_TOPIC::WATER_SET, buf, true);
        }
      }

      if (!initialClimateStatesPublished && desiredWaterC != PureSpaIO::UNDEF::INT)
      {
        mqttClient.publish(
          MQTT_TOPIC::CLIMATE_MODE,
          heaterOnOrStandby ? "heat" : "off",
          true,
          true
        );
        mqttClient.publish(
          MQTT_TOPIC::CLIMATE_ACTION,
          heaterOnOrStandby
            ? (heaterIsStandby ? "idle" : "heating")
            : "off",
          true,
          true
        );
        snprintf(buf, BUFFER_SIZE, "%d", desiredWaterC);
        mqttClient.publish(MQTT_TOPIC::WATER_SET, buf, true, true);
        initialClimateStatesPublished = true;
      }

#ifdef SERIAL_DEBUG
      publishIfDefined("pool/telegram/led", pureSpaIO.getRawLedValue(), PureSpaIO::UNDEF::USHORT);
#endif

      String errorCode = pureSpaIO.getErrorCode();
      if (errorCode.length())
      {
        mqttClient.publish(MQTT_TOPIC::ERROR, pureSpaIO.getErrorMessage(errorCode), retainAll, true);
      }
      else
      {
        mqttClient.publish(MQTT_TOPIC::ERROR, "", retainAll, true);
      }
      // Keep availability retained so HA always knows current device presence.
      mqttClient.publish(MQTT_TOPIC::STATE, "online", true, forcedStateUpdate);
    }
    else
    {
      g_wasSpaOnline = false;
      mqttClient.publish(MQTT_TOPIC::STATE, "offline", true, forcedStateUpdate);
    }

    if (wifiStateUpdateTime == 0
        || timeDiff(now, wifiStateUpdateTime) >= CONFIG::WIFI_UPDATE_PERIOD)
    {
      // Do not advance wifiStateUpdateTime until MQTT is up — otherwise a failed publish
      // (broker not connected yet) skips the real first send for WIFI_UPDATE_PERIOD.
      if (mqttClient.isConnected())
      {
        publishWifiDiagnostics(now);
      }
    }
  }
}
