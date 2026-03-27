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
using IntexWebServer = WebServer;

class WebConfig
{
public:
  void begin();
  void loop();
  bool isActive() const;

private:
  IntexWebServer server{80};
  bool active = false;
  bool apStarted = false;
  bool updateSuccess = false;

  void ensureFallbackAP();
  void stopFallbackAPIfConnected();
  void handleRoot();
  void handleScan();
  void handleSave();
  void handleUpdateFinished();
  void handleUpdateUpload();
};
