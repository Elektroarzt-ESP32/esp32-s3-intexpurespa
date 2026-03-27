# ESP32 Intex PureSpa SB-H20 WiFi Controller

This project is a modified version of the original ESP8266 implementation:
https://github.com/jnsbyr/esp8266-intexsbh20

The goal was to port the project to ESP32, make temperature control functional, simplify usage, and improve integration with Home Assistant.

---

## Main changes compared to the original version

* ESP32 support instead of ESP8266 (due to higher performance requirements)
* improved communication stability
* MQTT integration with Home Assistant autodiscovery support
* added climate entity for temperature control in Home Assistant
* web interface for device configuration
* single universal firmware (no need to compile for different models)
* improved value synchronization after device startup

---

## Wiring (ESP32)

The wiring is based on the original project but adapted for ESP32.

### Used pins

* GPIO18 – CLOCK
* GPIO19 – DATA
* GPIO23 – LATCH
* GPIO34 – NTC

### Power supply

* 5V / VIN depending on the ESP32 module used
* the spa control panel appears to provide sufficient current reserve
* recommended to replace the PTC fuse with at least 500 mA (originally ~200 mA)

### Note

Wiring may vary depending on the specific spa model and ESP32 board used.

---

## Home Assistant

The device uses MQTT autodiscovery, so entities are created automatically after connection.

### Typical entities

* climate entity for temperature control
* current water temperature
* target water temperature
* spa status (power, filtration, heating, etc.)

### Climate behavior

* `heat` mode turns on the spa (if it is off) and enables heating
* `off` mode disables heating

---

## Firmware

Prebuilt firmware is available in the **Releases** section.

* download `.bin` file
* upload to ESP32 (via OTA or esptool)

---

## Installation

1. Upload the firmware (.bin) to ESP32
2. After boot, the device creates a WiFi AP (192.168.4.1)
3. Open the web interface
4. Configure WiFi, MQTT server, spa model and other settings
5. The device will automatically appear in Home Assistant

---

## Features

* spa power on/off
* control of filtration, heating, bubbles, etc.
* reading current water temperature
* setting target temperature
* MQTT communication
* Home Assistant integration
* OTA firmware updates via web interface
* full web-based configuration

---

## Notes

This project was created as a practical modification for personal use.
Behavior may differ slightly from the original implementation.

Tested on SB-H20 model (short-term), stable so far.

---

## Original project

Author: Jens B.
Repository: https://github.com/jnsbyr/esp8266-intexsbh20

License remains unchanged according to the original project (CC BY-NC-SA 4.0).

---
