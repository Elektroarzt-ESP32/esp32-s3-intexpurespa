/*
 * project:  Intex PureSpa WiFi Controller
 *
 * file:     WebConfig.cpp
 *
 * Web configuration UI: Wi-Fi credentials, MQTT and discovery settings, optional
 * firmware OTA upload, and fallback access-point when the device is not on STA.
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

#include "WebConfig.h"
#include "ConfigurationFile.h"

#include <WiFi.h>
#include <Update.h>
#include "common.h"

extern ConfigurationFile config;

namespace
{
  String htmlEscape(const String& s)
  {
    String out;
    out.reserve(s.length() + 16);
    for (size_t i = 0; i < s.length(); i++)
    {
      const char c = s[i];
      switch (c)
      {
        case '&': out += F("&amp;"); break;
        case '<': out += F("&lt;"); break;
        case '>': out += F("&gt;"); break;
        case '"': out += F("&quot;"); break;
        case '\'': out += F("&#39;"); break;
        default: out += c; break;
      }
    }
    return out;
  }

  String buildRestartPage(const String& title, const String& message)
  {
    String html;
    html.reserve(3000);

    html += F("<!doctype html><html><head><meta charset='utf-8'>"
              "<meta name='viewport' content='width=device-width,initial-scale=1'>"
              "<style>"
              "body{font-family:Arial;background:#0b1220;color:#e5e7eb;display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0}"
              ".box{background:#1e293b;padding:24px;border-radius:18px;border:1px solid #334155;max-width:560px;text-align:center}"
              ".muted{color:#94a3b8}"
              "</style></head><body><div class='box'><h2>");
    html += htmlEscape(title);
    html += F("</h2><p>");
    html += htmlEscape(message);
    html += F("<script>"
              "setTimeout(function(){"
              "  const tryRoot=function(){"
              "    fetch('/', {cache:'no-store'})"
              "      .then(function(r){ if(r.ok){ window.location.href='/'; } else { setTimeout(tryRoot, 1500); } })"
              "      .catch(function(){ setTimeout(tryRoot, 1500); });"
              "  };"
              "  tryRoot();"
              "}, 2500);"
              "</script></body></html>");

    return html;
  }

  String buildModelLabel(const String& selectedModel)
  {
    String m = selectedModel;
    m.toUpperCase();
    if (m.indexOf("SJB") >= 0 || m.indexOf("HS") >= 0)
    {
      return F("SJB-HS");
    }

    return F("SB-H20 / SSP-H-20-1 / SB-B20");
  }
}

void WebConfig::begin()
{
  if (active)
  {
    return;
  }

  WiFi.mode(WIFI_AP_STA);

  server.on("/", HTTP_GET, std::bind(&WebConfig::handleRoot, this));
  server.on("/scan", HTTP_GET, std::bind(&WebConfig::handleScan, this));
  server.on("/save", HTTP_POST, std::bind(&WebConfig::handleSave, this));
  server.on("/update", HTTP_POST,
            std::bind(&WebConfig::handleUpdateFinished, this),
            std::bind(&WebConfig::handleUpdateUpload, this));

  server.begin();
  active = true;
  ensureFallbackAP();
}

void WebConfig::loop()
{
  if (!active)
  {
    return;
  }

  server.handleClient();

  if (WiFi.status() == WL_CONNECTED)
  {
    stopFallbackAPIfConnected();
  }
  else
  {
    ensureFallbackAP();
  }
}

bool WebConfig::isActive() const
{
  return active;
}

void WebConfig::ensureFallbackAP()
{
  if (!apStarted)
  {
    WiFi.softAP(CONFIG::DEFAULT_AP_NAME);
    apStarted = true;
  }
}

void WebConfig::stopFallbackAPIfConnected()
{
  if (apStarted)
  {
    WiFi.softAPdisconnect(true);
    apStarted = false;
  }
}

void WebConfig::handleRoot()
{
  String wifiSsid        = htmlEscape(String(config.get(CONFIG_TAG::WIFI_SSID)));
  String wifiPass        = htmlEscape(String(config.get(CONFIG_TAG::WIFI_PASSPHRASE)));
  String mqttServer      = htmlEscape(String(config.get(CONFIG_TAG::MQTT_SERVER)));
  String mqttPort        = htmlEscape(String(config.get(CONFIG_TAG::MQTT_PORT)));
  String mqttUser        = htmlEscape(String(config.get(CONFIG_TAG::MQTT_USER)));
  String mqttPassword    = htmlEscape(String(config.get(CONFIG_TAG::MQTT_PASSWORD)));
  String mqttRetain      = String(config.get(CONFIG_TAG::MQTT_RETAIN));
  String errorLanguage   = String(config.get(CONFIG_TAG::MQTT_ERROR_LANG));
  String firmwareUrl     = htmlEscape(String(config.get(CONFIG_TAG::WIFI_OTA_URL)));
  String discoveryMode   = String(config.get(CONFIG_TAG::MQTT_DISCOVERY_MODE));
  String discoveryPrefix = htmlEscape(String(config.get(CONFIG_TAG::MQTT_DISCOVERY_PREFIX)));
  String customModelName = htmlEscape(String(config.get(CONFIG_TAG::CUSTOM_MODEL_NAME_KEY)));
  String forceWifiSleep  = String(config.get(CONFIG_TAG::FORCE_WIFI_SLEEP_KEY));
  String selectedModel   = String(config.get(CONFIG_TAG::SELECTED_MODEL_KEY));

  if (!mqttRetain.length()) mqttRetain = "no";
  if (!errorLanguage.length()) errorLanguage = "EN";
  if (!mqttPort.length()) mqttPort = CONFIG::DEFAULT_MQTT_PORT;
  if (!forceWifiSleep.length()) forceWifiSleep = CONFIG::DEFAULT_FORCE_WIFI_SLEEP;
  if (!discoveryMode.length()) discoveryMode = CONFIG::DEFAULT_MQTT_DISCOVERY_MODE;
  if (!discoveryPrefix.length()) discoveryPrefix = CONFIG::DEFAULT_MQTT_DISCOVERY;
  if (!customModelName.length()) customModelName = CONFIG::DEFAULT_MODEL_NAME;
  if (!selectedModel.length()) selectedModel = CONFIG::DEFAULT_SELECTED_MODEL;

  String html;
  html.reserve(28000);
  html += F(R"HTML(<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Intex PureSpa Setup</title><style>
:root{--bg:#0b1220;--panel:#1e293b;--panel2:#0f172a;--text:#e5e7eb;--muted:#94a3b8;--line:#334155;--accent:#2563eb;--accent2:#3b82f6}
*{box-sizing:border-box}body{margin:0;font-family:Arial,Helvetica,sans-serif;background:linear-gradient(180deg,#0b1220 0%,#0a1428 100%);color:var(--text)}
.wrap{max-width:980px;margin:0 auto;padding:28px 20px 56px}.stack{display:flex;flex-direction:column;gap:18px}
.card{background:var(--panel);border:1px solid rgba(148,163,184,.18);border-radius:20px;padding:18px;box-shadow:0 10px 30px rgba(0,0,0,.22)}
h1{margin:0 0 18px;font-size:24px;font-weight:700}.card h2{margin:0 0 12px;font-size:20px}.note,.msg,.scanstatus{color:var(--muted)}
.badges{display:flex;gap:10px;flex-wrap:wrap;margin-bottom:12px}.badge{background:#0b1733;border:1px solid #35508b;color:#fff;padding:6px 10px;border-radius:999px;font-size:13px}
label{display:block;margin:14px 0 8px;color:#cbd5e1}input,select{width:100%;background:var(--panel2);color:var(--text);border:1px solid var(--line);border-radius:12px;padding:12px 14px;outline:none}input:focus,select:focus{border-color:var(--accent2);box-shadow:0 0 0 3px rgba(59,130,246,.15)}input[readonly]{opacity:.8}
button{appearance:none;border:0;border-radius:12px;background:linear-gradient(180deg,var(--accent2),var(--accent));color:#fff;padding:12px 18px;font-weight:700;cursor:pointer}button:hover{filter:brightness(1.06)}
.row2{display:grid;grid-template-columns:1fr 1fr;gap:14px}@media (max-width: 700px){.row2{grid-template-columns:1fr}}.list{margin-top:16px;display:flex;flex-direction:column;gap:10px}.net{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:12px 14px;background:var(--panel2);border:1px solid var(--line);border-radius:12px;cursor:pointer}.net:hover{border-color:var(--accent2)}.ssid{font-weight:600;word-break:break-all}.meta{font-size:13px;color:var(--muted)}.footer{display:flex;align-items:center;justify-content:space-between;gap:14px;margin-top:18px;flex-wrap:wrap}
small{display:block;color:var(--muted);margin-top:6px}.sectionline{margin:18px 0;border:0;border-top:1px solid rgba(148,163,184,.18)}
</style></head><body><div class="wrap"><h1>Intex PureSpa Setup</h1><div class="stack"><div class="card"><h2>Status &amp; Wi-Fi</h2><div class="badges"><div class="badge">Selected model: )HTML");
  html += htmlEscape(buildModelLabel(selectedModel));
  html += F(R"HTML(</div><div class="badge">FW: )HTML");
  html += htmlEscape(CONFIG::WIFI_VERSION);
  html += F(R"HTML(</div></div><p class="note">Active spa model is selected in this page. Changes require a restart.</p><button type="button" onclick="scanWifi()">Scan Wi-Fi</button><p id="scanStatus" class="scanstatus" style="margin:16px 0 0">No scan yet.</p><div id="networks" class="list"></div></div><div class="card"><h2>Settings</h2><form method="post" action="/save"><label for="customModelName">MQTT Device Name</label><input id="customModelName" name=")HTML");
  html += CONFIG_TAG::CUSTOM_MODEL_NAME_KEY;
  html += F(R"HTML(" value=")HTML");
  html += customModelName;
  html += F(R"HTML("><small>Device name used in MQTT / Home Assistant discovery.</small><label for="wifiSSID">Wi-Fi SSID</label><input id="wifiSSID" name=")HTML");
  html += CONFIG_TAG::WIFI_SSID;
  html += F(R"HTML(" value=")HTML");
  html += wifiSsid;
  html += F(R"HTML("><label for="wifiPassphrase">Wi-Fi Password</label><input id="wifiPassphrase" name=")HTML");
  html += CONFIG_TAG::WIFI_PASSPHRASE;
  html += F(R"HTML(" type="password" value=")HTML");
  html += wifiPass;
  html += F(R"HTML("><div class="row2"><div><label for="mqttServer">MQTT Server</label><input id="mqttServer" name=")HTML");
  html += CONFIG_TAG::MQTT_SERVER;
  html += F(R"HTML(" value=")HTML");
  html += mqttServer;
  html += F(R"HTML("></div><div><label for="mqttPort">MQTT Port</label><input id="mqttPort" name=")HTML");
  html += CONFIG_TAG::MQTT_PORT;
  html += F(R"HTML(" value=")HTML");
  html += mqttPort;
  html += F(R"HTML("></div></div><div class="row2"><div><label for="mqttRetain">MQTT Retain</label><select id="mqttRetain" name=")HTML");
  html += CONFIG_TAG::MQTT_RETAIN;
  html += F(R"HTML("><option value="no")HTML");
  html += (mqttRetain == "no" ? " selected" : "");
  html += F(R"HTML(>no</option><option value="yes")HTML");
  html += (mqttRetain == "yes" ? " selected" : "");
  html += F(R"HTML(>yes</option></select></div><div><label for="mqttUser">MQTT User</label><input id="mqttUser" name=")HTML");
  html += CONFIG_TAG::MQTT_USER;
  html += F(R"HTML(" value=")HTML");
  html += mqttUser;
  html += F(R"HTML("></div></div><label for="mqttPassword">MQTT Password</label><input id="mqttPassword" name=")HTML");
  html += CONFIG_TAG::MQTT_PASSWORD;
  html += F(R"HTML(" type="password" value=")HTML");
  html += mqttPassword;
  html += F(R"HTML("><div class="row2"><div><label for="mqttDiscoveryMode">MQTT Mode</label><select id="mqttDiscoveryMode" name=")HTML");
  html += CONFIG_TAG::MQTT_DISCOVERY_MODE;
  html += F(R"HTML(" onchange="onMqttModeChange()"><option value="HA")HTML");
  html += (discoveryMode == "HA" ? " selected" : "");
  html += F(R"HTML(>HA discovery</option><option value="DIRECT")HTML");
  html += (discoveryMode == "DIRECT" ? " selected" : "");
  html += F(R"HTML(>Direct only</option></select><small>Usually homeassistant. In Direct only mode, discovery is not published.</small></div><div><label for="mqttDiscoveryPrefix">HA Discovery Prefix</label><input id="mqttDiscoveryPrefix" name=")HTML");
  html += CONFIG_TAG::MQTT_DISCOVERY_PREFIX;
  html += F(R"HTML(" value=")HTML");
  html += discoveryPrefix;
  html += F(R"HTML("></div></div><div class="row2"><div><label for="errorLanguage">Error Language</label><select id="errorLanguage" name=")HTML");
  html += CONFIG_TAG::MQTT_ERROR_LANG;
  html += F(R"HTML("><option value="EN")HTML");
  html += (errorLanguage == "EN" ? " selected" : "");
  html += F(R"HTML(>EN</option><option value="DE")HTML");
  html += (errorLanguage == "DE" ? " selected" : "");
  html += F(R"HTML(>DE</option><option value="CZ")HTML");
  html += (errorLanguage == "CZ" ? " selected" : "");
  html += F(R"HTML(>CZ</option><option value="CODE")HTML");
  html += (errorLanguage == "CODE" ? " selected" : "");
  html += F(R"HTML(>CODE</option></select></div><div><label for="forceWifiSleep">Force Wi-Fi Sleep</label><select id="forceWifiSleep" name=")HTML");
  html += CONFIG_TAG::FORCE_WIFI_SLEEP_KEY;
  html += F(R"HTML("><option value="yes")HTML");
  html += (forceWifiSleep == "yes" ? " selected" : "");
  html += F(R"HTML(>yes</option><option value="no")HTML");
  html += (forceWifiSleep == "no" ? " selected" : "");
  html += F(R"HTML(>no</option></select></div></div><label for="selectedModel">Spa Model</label><select id="selectedModel" name=")HTML");
  html += CONFIG_TAG::SELECTED_MODEL_KEY;
  html += F(R"HTML("><option value="SB-H20")HTML");
  html += (selectedModel == "SB-H20" ? " selected" : "");
  html += F(R"HTML(>SB-H20 / SSP-H-20-1 / SB-B20</option><option value="SJB-HS")HTML");
  html += (selectedModel == "SJB-HS" ? " selected" : "");
  html += F(R"HTML(>SJB-HS</option></select><label for="firmwareURL">Firmware URL</label><input id="firmwareURL" name=")HTML");
  html += CONFIG_TAG::WIFI_OTA_URL;
  html += F(R"HTML(" value=")HTML");
  html += firmwareUrl;
  html += F(R"HTML("><small>This keeps OTA updates from Home Assistant via URL working.</small><div class="footer"><button type="submit">Save &amp; Restart</button><span class="msg">Configuration is stored in flash memory. No LittleFS / config.json is needed.</span></div></form><hr class="sectionline"><h2>OTA Update</h2><form method="post" action="/update" enctype="multipart/form-data"><label for="updateFile">Upload firmware BIN</label><input id="updateFile" name="update" type="file" accept=".bin,application/octet-stream"><small>This updates directly from your browser. URL based OTA remains available too.</small><div class="footer"><button type="submit">Upload &amp; Flash</button></div></form></div></div></div><script>
function scanWifi(){const status=document.getElementById('scanStatus');const list=document.getElementById('networks');status.textContent='Scanning networks...';list.innerHTML='';fetch('/scan').then(r=>r.json()).then(data=>{if(!Array.isArray(data)||!data.length){status.textContent='No networks found.';return;}status.textContent='Select a network from the list.';data.forEach(item=>{const el=document.createElement('div');el.className='net';const left=document.createElement('div');const ssid=document.createElement('div');ssid.className='ssid';ssid.textContent=item.ssid||'(hidden network)';const meta=document.createElement('div');meta.className='meta';meta.textContent='RSSI: '+item.rssi+' dBm • '+(item.secured?'secured':'open');left.appendChild(ssid);left.appendChild(meta);el.appendChild(left);el.onclick=function(){document.getElementById('wifiSSID').value=item.ssid||'';};list.appendChild(el);});}).catch(()=>{status.textContent='Scan failed.';});}
function onMqttModeChange(){const mode=document.getElementById('mqttDiscoveryMode').value;const prefix=document.getElementById('mqttDiscoveryPrefix');if(mode==='HA'){prefix.value='homeassistant';prefix.readOnly=true;}else{prefix.readOnly=false;}}
window.addEventListener('load', onMqttModeChange);
</script></body></html>)HTML");

  server.send(200, "text/html; charset=utf-8", html);
}

void WebConfig::handleScan()
{
  int count = WiFi.scanNetworks(false, true);

  String json = "[";
  for (int i = 0; i < count; i++)
  {
    if (i > 0)
    {
      json += ",";
    }

    json += "{\"ssid\":\"";
    String ssid = WiFi.SSID(i);
    for (size_t j = 0; j < ssid.length(); j++)
    {
      const char c = ssid[j];
      if (c == '"' || c == '\\')
      {
        json += '\\';
      }
      json += c;
    }
    json += "\",\"rssi\":";
    json += String(WiFi.RSSI(i));
    json += ",\"secured\":";
    json += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true");
    json += "}";
  }
  json += "]";

  server.send(200, "application/json; charset=utf-8", json);
}

void WebConfig::handleSave()
{
  String wifiSsid        = server.arg(CONFIG_TAG::WIFI_SSID);
  String wifiPass        = server.arg(CONFIG_TAG::WIFI_PASSPHRASE);
  String mqttServer      = server.arg(CONFIG_TAG::MQTT_SERVER);
  String mqttPort        = server.arg(CONFIG_TAG::MQTT_PORT);
  String mqttUser        = server.arg(CONFIG_TAG::MQTT_USER);
  String mqttPass        = server.arg(CONFIG_TAG::MQTT_PASSWORD);
  String mqttRetain      = server.arg(CONFIG_TAG::MQTT_RETAIN);
  String errorLanguage   = server.arg(CONFIG_TAG::MQTT_ERROR_LANG);
  String firmwareUrl     = server.arg(CONFIG_TAG::WIFI_OTA_URL);
  String discoveryMode   = server.arg(CONFIG_TAG::MQTT_DISCOVERY_MODE);
  String discoveryPrefix = server.arg(CONFIG_TAG::MQTT_DISCOVERY_PREFIX);
  String customModelName = server.arg(CONFIG_TAG::CUSTOM_MODEL_NAME_KEY);
  String forceWifiSleep  = server.arg(CONFIG_TAG::FORCE_WIFI_SLEEP_KEY);
  String selectedModel   = server.arg(CONFIG_TAG::SELECTED_MODEL_KEY);

  if (!mqttPort.length())         mqttPort = CONFIG::DEFAULT_MQTT_PORT;
  if (!discoveryMode.length())    discoveryMode = CONFIG::DEFAULT_MQTT_DISCOVERY_MODE;
  if (discoveryMode == "HA")      discoveryPrefix = CONFIG::DEFAULT_MQTT_DISCOVERY;
  if (!discoveryPrefix.length())  discoveryPrefix = CONFIG::DEFAULT_MQTT_DISCOVERY;
  if (!customModelName.length())  customModelName = CONFIG::DEFAULT_MODEL_NAME;
  if (!forceWifiSleep.length())   forceWifiSleep = CONFIG::DEFAULT_FORCE_WIFI_SLEEP;
  if (!selectedModel.length())   selectedModel = CONFIG::DEFAULT_SELECTED_MODEL;
  if (!mqttRetain.length())       mqttRetain = "no";
  if (!errorLanguage.length())    errorLanguage = "EN";

  config.set(CONFIG_TAG::WIFI_SSID,             wifiSsid);
  config.set(CONFIG_TAG::WIFI_PASSPHRASE,       wifiPass);
  config.set(CONFIG_TAG::MQTT_SERVER,           mqttServer);
  config.set(CONFIG_TAG::MQTT_PORT,             mqttPort);
  config.set(CONFIG_TAG::MQTT_USER,             mqttUser);
  config.set(CONFIG_TAG::MQTT_PASSWORD,         mqttPass);
  config.set(CONFIG_TAG::MQTT_RETAIN,           mqttRetain);
  config.set(CONFIG_TAG::MQTT_ERROR_LANG,       errorLanguage);
  config.set(CONFIG_TAG::MQTT_DISCOVERY_MODE,   discoveryMode);
  config.set(CONFIG_TAG::MQTT_DISCOVERY_PREFIX, discoveryPrefix);
  config.set(CONFIG_TAG::CUSTOM_MODEL_NAME_KEY, customModelName);
  config.set(CONFIG_TAG::SELECTED_MODEL_KEY,   selectedModel);
  config.set(CONFIG_TAG::FORCE_WIFI_SLEEP_KEY,  forceWifiSleep);
  config.set(CONFIG_TAG::WIFI_OTA_URL,          firmwareUrl);

  if (!config.save())
  {
    server.send(500, "text/plain; charset=utf-8", "Failed to save config");
    return;
  }

  server.send(200, "text/html; charset=utf-8",
              buildRestartPage("Saved", "Configuration was stored. The device will restart now..."));

  delay(1000);
  ESP.restart();
}

void WebConfig::handleUpdateFinished()
{
  if (updateSuccess)
  {
    server.send(200, "text/html; charset=utf-8",
                buildRestartPage("OTA update uploaded", "Firmware was written successfully. The device will reboot now..."));
    delay(1000);
    ESP.restart();
  }
  else
  {
    String err = Update.errorString();
    server.send(500, "text/plain; charset=utf-8", err);
  }
}

void WebConfig::handleUpdateUpload()
{
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START)
  {
    updateSuccess = false;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
    {
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
    {
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (Update.end(true))
    {
      updateSuccess = true;
    }
    else
    {
      Update.printError(Serial);
    }
  }
}