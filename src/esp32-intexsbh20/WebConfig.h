/*
 * project:  Intex PureSpa WiFi Controller
 *
 * file:     WebConfig.h
 *
 * Declarations for the embedded web server used to configure Wi-Fi, MQTT,
 * discovery, and OTA from a browser (see WebConfig.cpp).
 *
 * encoding: UTF-8
 * created:  27 March 2026
 *
 * Copyright (C) 2026 Petr Kašpar
 *
 * Author: Petr Kašpar
 * https://github.com/caspercze
 *
 */

#pragma once

#include <WebServer.h>
#include <DNSServer.h>
using IntexWebServer = WebServer;

class WebConfig
{
public:
  void begin();
  void loop();
  bool isActive() const;

private:
  IntexWebServer server{80};
  DNSServer dnsServer;
  bool active = false;
  bool apStarted = false;
  bool updateSuccess = false;

  void ensureFallbackAP();
  void stopFallbackAPIfConnected();
  void handleCaptivePortal();
  void handleRoot();
  void handleScan();
  void handleSave();
  void handleResetCredentials();
  void handleUpdateFinished();
  void handleUpdateUpload();
};
