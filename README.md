# ESP32-S3 Intex PureSpa SB-H20 WiFi Controller

> This is an unofficial ESP32-S3 fork of the original project.

## Credits

Original project by Jens B.:
https://github.com/jnsbyr/esp8266-intexsbh20

Based in part on DIYSCIP by Geoffroy Hubert:
https://github.com/yorffoeg/diyscip

ESP32 port by Petr Kašpar (caspercze):
https://github.com/caspercze

This project is an ESP32-S3-based modification of the ESP32 port by caspercze, which in
turn is based on the original ESP8266 project by Jens B.

The goal was to port the project to ESP32-S3, make temperature control functional.

---

## Main changes compared to the original version

* ESP32-S3 support (ported from ESP8266 via caspercze's ESP32 port)
* improved communication stability
* MQTT integration with Home Assistant autodiscovery support
* added climate entity for temperature control in Home Assistant
* web interface for device configuration
* single universal firmware (no need to compile for different models)
* improved value synchronization after device startup
* topic change to Intex_PureSpa/...
* ADC calibration via `analogReadMilliVolts()` for improved ESP32 temp accuracy

<img width="1539" height="728" alt="image" src="https://github.com/user-attachments/assets/e652155e-91c6-47a2-8710-2870890663e0" />

<img width="1537" height="725" alt="image" src="https://github.com/user-attachments/assets/6a9c1ec8-b0d8-49be-bd6a-f02c3cef36de" />

<img width="1542" height="722" alt="image" src="https://github.com/user-attachments/assets/6180a13d-32bf-4b11-adda-a120388165e3" />

<img width="343" height="421" alt="image" src="https://github.com/user-attachments/assets/fa953c9c-9729-4b01-835d-9a815e349c5d" />

<img width="805" height="732" alt="image" src="https://github.com/user-attachments/assets/567c9d63-2565-40b7-9cf1-c2910820581d" />

<img width="809" height="716" alt="image" src="https://github.com/user-attachments/assets/03ba1a92-94e4-4997-bd71-4ac83995f1d1" />


---

## Wiring (ESP32-S3)

The wiring is based on the original project but adapted for ESP32-S3.

### Used pins

* GPIO6  – CLOCK
* GPIO7  – DATA
* GPIO8  – LATCH
* GPIO4  – NTC

### Power supply

* 5V / VIN
* the spa control panel appears to provide sufficient current reserve
* PTC fuse with 1000 mA (originally ~200 mA)

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

* download `esp32-s3-intexsbh20.bin`
* upload to ESP32-S3 (via OTA or esptool)

---

## Installation

1. Upload `esp32-s3-intexsbh20.bin` to ESP32-S3
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

This project aimed at providing a ESP32-S3-based PCB solution.
Behavior may differ slightly from the original implementation.

Tested on SB-H20 model (short-term), stable so far.

See [CHANGELOG.md](CHANGELOG.md) for a detailed list of changes per release.

---

## License

This project is a derivative work based on:
https://github.com/jnsbyr/esp8266-intexsbh20

It includes code from multiple sources:

- Original project by Jens B. (see repository for license details)
- DIYSCIP by Geoffroy Hubert (licensed under CC BY-NC-SA 4.0)
- ESP32 port by Petr Kašpar (caspercze)

Therefore, this project contains components under different licenses.

For non-commercial use, this repository follows the terms of:
Creative Commons Attribution-NonCommercial-ShareAlike 4.0 (CC BY-NC-SA 4.0)

Please refer to the original projects for full license details.
