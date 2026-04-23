# Changelog

## [1.4.1-esp32] – 2026-04-23

### Port to ESP32-S3

This release documents all changes required to run the firmware on an ESP32-S3, replacing
the original GPIO assignment for a legacy ESP32 and fixing several compatibility issues
with Arduino Core for ESP32 v5.x (IDF5-based).

---

### Firmware changes

#### GPIO assignment (`common.h`)

GPIO numbers updated to match the ESP32-S3 target hardware.

| Signal | Old (ESP32) | New (ESP32-S3) |
|--------|-------------|----------------|
| CLOCK  | GPIO 18     | GPIO 6         |
| DATA   | GPIO 19     | GPIO 7         |
| LATCH  | GPIO 23     | GPIO 8         |
| NTC    | GPIO 34     | GPIO 4         |

#### SoftAP startup (`WebConfig.cpp`)

`WiFi.softAP()` did not start reliably on ESP32-S3 with Arduino Core v5.x without a
settling delay. Fixed by adding a 100 ms delay before the `softAP()` call, plus a
success/failure log message on the serial console.

#### Duplicate `WiFi.mode()` call removed (`esp32-intexsbh20.ino`)

A second `WiFi.mode(WIFI_AP_STA)` call in `setup()` after `webConfig.begin()` reset the
already-running SoftAP on ESP32-S3/Arduino Core v5, making the web configuration
inaccessible. The redundant call has been removed.

#### NTC temperature measurement (`NTCThermometer.cpp`, `esp32-intexsbh20.ino`)

Three issues were fixed:

1. **`adcScale` parameter was ignored:** The original implementation calculated
   `this->adcScale = refVoltage / 4095.0f`, discarding the passed `adcScale` argument.
   Corrected to `this->adcScale = (refVoltage * adcScale) / 4095.0f`.

2. **Wrong `adcScale` value:** The call `thermometer.setup(22000, 3.30f, 320.f/100.f)`
   assumed an external voltage divider between the NTC output and the MCU pin. The PCB
   schematic shows the NTC junction (TH1 10 kΩ + R2 22 kΩ to GND) connects **directly**
   to the MCU pin with no additional divider. Corrected to `1.0f`.

3. **ADC calibration:** `analogRead()` replaced by `analogReadMilliVolts()`, which applies
   the factory ADC calibration stored in eFuse (Arduino-ESP32 v3+). This reduces ADC
   non-linearity from ±6 % to ±1–2 %. Overall accuracy is approximately ±2–3 °C,
   limited by the NTC tolerance class J (±5 %).

#### `ESP.getFreeHeap()` cast (`MQTTPublisher.cpp`)

Added explicit cast to `unsigned int` to suppress compiler warnings under ESP32/IDF5.

#### Serial debug output (`esp32-intexsbh20.ino`)

- PIN assignment and free heap printed to serial console on startup.
- Log message when the spa panel comes online for the first time (includes frame counter).
- `SERIAL_DEBUG` enabled in `common.h`.

---

## [1.4.0-esp32]

Initial ESP32 port by Petr Kašpar (caspercze).
See README and git history for details.
