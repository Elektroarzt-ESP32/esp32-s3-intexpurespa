# Changelog

## [1.4.3-esp32-s3] – 2026-06-21

### Fix: unreliable water temperature setting (`PureSpaIO.cpp`)

**Symptom:** Power on/off via Home Assistant worked reliably, but setting the
water temperature did not. Multi-degree changes (e.g. +4 °C) were frequently
applied only partially (1–2 °C), the time per 1 °C step varied wildly
(~0.5 s up to ~3 s), and afterwards Home Assistant's reported target
temperature no longer matched the panel/local display.

**Root cause:** In `decodeDisplay()`, a blinking display value within ±1 °C of
the current actual water temperature was unconditionally treated as the
panel's own "blink the actual temp" quirk and written to `state.waterTemp`
instead of `state.desiredTemp`. During an active temp button sequence this
shortcut fired on perfectly normal new setpoints (raising/lowering the target
a few degrees very commonly lands within 1 °C of the actual temp at some
point), so `state.desiredTemp` was never updated for that step.
`setDesiredWaterTempCelsius()`'s confirm step polls `getDesiredWaterTempCelsius()`
(backed by `state.desiredTemp`) for up to 3 s, so a misclassified step always
timed out, triggered one retry (a second real button press), and after two
failures the whole multi-degree request aborted via `break` — explaining both
the partial application and the 0.5 s vs. 3 s timing spread. Because each
button press genuinely changes the spa's physical setpoint as soon as the
panel ACKs it (the buzzer), independent of whether the software's readback
ever confirmed it, failed-confirm presses left the real hardware setpoint
ahead of what `g_lastAcceptedDesiredFromIsrC` (and thus the `WATER_SET` MQTT
topic published to Home Assistant) believed — explaining the persistent
desync between Home Assistant and the local display after an adjustment.

**Fix:** The ±1 °C "treat as actual temp" shortcut in `decodeDisplay()` is now
only applied when there has been no temp button press in the last 12 s
(`g_lastTempUiActionFrame`, the same window already used by
`shouldRejectBlinkAsSetpoint()`, now shared via the new
`RECENT_TEMP_UI_ACTION_MS` constant). During an active button-driven
adjustment, the blinking value is routed to the existing, more precise
`shouldRejectBlinkAsSetpoint()` heuristic instead, which already
distinguishes a genuine new setpoint from a coincidental actual-temp-shaped
blink. The original protection (panel spontaneously blinking the actual
temperature with no button involved) is unchanged.

---

## [1.4.2-esp32-s3] – 2026-05-07

### Web interface and connectivity improvements

---

#### Captive portal (`WebConfig.cpp`, `WebConfig.h`)

When a device connects to the ESP32-S3 access point, the operating system now
opens the setup page automatically without requiring manual navigation to
`192.168.4.1`.

Implementation:
- A `DNSServer` instance resolves all hostnames to the AP IP (`192.168.4.1`).
- OS-specific captive portal detection endpoints are registered and redirect to
  the setup page: `/generate_204` and `/gen_204` (Android/Chrome),
  `/hotspot-detect.html` and `/library/test/success.html` (Apple),
  `/connecttest.txt` and `/ncsi.txt` (Windows), `/success.txt` (Firefox).
- The DNS server starts with the AP and stops when the device connects to
  the home network.

#### WiFi hostname (`WebConfig.cpp`)

`WiFi.setHostname()` is now called before `WiFi.mode()` so the device appears
as `Intex_PureSpa` instead of the default `esp32s3-XXXXXX` in the router's
device list and mDNS.

#### Web interface: Reset Credentials button (`WebConfig.cpp`)

A red **Reset Credentials** button clears the stored WiFi SSID, WiFi password,
MQTT server, MQTT user, and MQTT password. MQTT port and all other settings
(spa model, language, discovery prefix, etc.) are kept. The device restarts
after the reset. A browser confirmation dialog prevents accidental activation.

#### Web interface: select field height (`WebConfig.cpp`)

`<select>` elements now have the same height as `<input>` fields on all
platforms including macOS. Fixed by adding `height: 46px`,
`-webkit-appearance: none`, `appearance: none`, and a custom SVG chevron icon
as a CSS background image.

---

## [1.4.1-esp32-s3] – 2026-04-23

### Port to ESP32-S3

This release documents all changes required to run the firmware on an ESP32-S3,
replacing the original GPIO assignment for a legacy ESP32 and fixing several
compatibility issues with Arduino Core for ESP32 v5.x (IDF5-based).

---

#### GPIO assignment (`common.h`)

GPIO numbers updated to match the ESP32-S3 target hardware.

| Signal | Old (ESP32) | New (ESP32-S3) |
|--------|-------------|----------------|
| CLOCK  | GPIO 18     | GPIO 6         |
| DATA   | GPIO 19     | GPIO 7         |
| LATCH  | GPIO 23     | GPIO 8         |
| NTC    | GPIO 34     | GPIO 4         |

#### SoftAP startup (`WebConfig.cpp`)

`WiFi.softAP()` did not start reliably on ESP32-S3 with Arduino Core v5.x
without a settling delay. Fixed by adding a 100 ms delay before the `softAP()`
call, plus a success/failure log message on the serial console.

#### Duplicate `WiFi.mode()` call removed (`esp32-intexsbh20.ino`)

A second `WiFi.mode(WIFI_AP_STA)` call in `setup()` after `webConfig.begin()`
reset the already-running SoftAP on ESP32-S3/Arduino Core v5, making the web
configuration inaccessible. The redundant call has been removed.

#### NTC temperature measurement (`NTCThermometer.cpp`, `esp32-intexsbh20.ino`)

Three issues were fixed:

1. **`adcScale` parameter was ignored:** The original implementation calculated
   `this->adcScale = refVoltage / 4095.0f`, discarding the passed `adcScale`
   argument. Corrected to `this->adcScale = (refVoltage * adcScale) / 4095.0f`.

2. **Wrong `adcScale` value:** The call `thermometer.setup(22000, 3.30f, 320.f/100.f)`
   assumed an external voltage divider between the NTC output and the MCU pin.
   The PCB schematic shows the NTC junction (TH1 10 kΩ + R6 22 kΩ to GND)
   connects directly to the MCU pin with no additional divider. Corrected to
   `1.0f`.

3. **ADC reading on Arduino-ESP32 v5.x:** `analogSetPinAttenuation()` drives the
   ADC pin as a digital output LOW on Arduino-ESP32 v5.x instead of configuring
   attenuation, causing near-zero ADC readings. Removed. `analogReadMilliVolts()`
   returns 0 silently on v5.5.x when the internal eFuse calibration path fails.
   Replaced by `analogRead()` with manual conversion using the 3.3 V reference.
   Overall accuracy is approximately ±2–3 °C, limited by the NTC tolerance
   class J (±5 %). The deprecated `esp_adc_cal.h` include was also removed.

#### `ESP.getFreeHeap()` cast (`MQTTPublisher.cpp`)

Added explicit cast to `unsigned int` to suppress compiler warnings under
ESP32/IDF5.

#### Arduino IDE board settings

PSRAM must be set to **Disabled** in the Arduino IDE board settings
(Tools → PSRAM → Disabled). ESP32-S3 modules without PSRAM log
`E octal_psram: PSRAM chip is not connected` at boot if the binary was
compiled with PSRAM enabled, which causes a boot failure on some modules.
This project does not use PSRAM.

#### Serial debug output (`esp32-intexsbh20.ino`)

- PIN assignment and free heap printed to serial console on startup.
- Log message when the spa panel comes online for the first time (includes
  frame counter).
- `SERIAL_DEBUG` enabled in `common.h`.

---

## [1.4.0-esp32]

Initial ESP32 port by Petr Kašpar (caspercze).
See README and git history for details.
