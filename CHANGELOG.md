# Changelog

## [1.4.7-esp32-s3] – 2026-06-22

### Fix: direction guard for stable setpoint detection (`PureSpaIO.cpp`)

**Problem with 1.4.6:** The stable setpoint detection path introduced in 1.4.6
ran for ANY temperature value that passed `shouldRejectBlinkAsSetpoint`. On the
SB-H20, after a temp-up press, the display briefly shows the OLD setpoint (e.g.
29°C) before updating to the new one (30°C). Both values passed the reject-test
(1°C steps are always accepted), causing `state.desiredTemp` to oscillate
between 29 and 30 — and the per-step confirmation poll to see the old value on
half the polls, resulting in all steps after the first two failing.

**Fix:** Added `g_lastTempUiActionDirection` (set in `changeWaterTemp` after
buzzer ACK) and a `directionOk` guard in the stable detection path:
- Up press (+1): only accept values strictly HIGHER than `prevDC`
- Down press (-1): only accept values strictly LOWER than `prevDC`

This prevents the old setpoint value from overwriting a just-accepted new one,
making all steps in a multi-degree change confirm reliably.

---

## [1.4.6-esp32-s3] – 2026-06-22

### Fix: stable setpoint detection for SB-H20 display protocol (`PureSpaIO.cpp`)

**Root cause (found via MQTT debug build 1.4.5):**  
The ISR's setpoint detection relied exclusively on a "blinking" display path:
the spa panel was expected to alternate between showing the setpoint value
and a blank display. The SB-H20/SSP-H-20-1/SB-B20 models only show this
blank transition when *entering* setpoint display mode for the first time
(e.g. during boot discovery). Subsequent temperature button presses while
already in setpoint mode update the displayed value **stably, without a
blank phase**. Since `isDisplayBlinking` was never set to `true` for these
subsequent presses, `state.desiredTemp` was never updated, and the per-step
confirmation poll in `setDesiredWaterTempCelsius()` timed out on every step
after the first.

**Symptom:**  
- Multi-degree temperature changes worked only for the very first change after
  boot (or after a device restart with the spa in blink-entry mode).
- All subsequent changes silently failed: the buzzer confirmed the button press
  (`click=1`), but `confirmSetpointChange` ran 20 polls × 150 ms and found
  `state.desiredTemp` unchanged every time (`cand=prev`), then retried once
  more and aborted.

**Fix:**  
Added a stable-display setpoint detection path in `decodeDisplay()`. When:
- `isDisplayBlinking` is false (no blink/blank cycle detected), AND
- a recent temp UI action occurred within `TEMP_UI_WATER_SUPPRESS_MS` (3 s),
  AND
- the stable display shows a value in the valid setpoint range
  [SET_MIN, SET_MAX], AND
- `shouldRejectBlinkAsSetpoint()` approves it

then `state.desiredTemp`, `g_lastKnownSetTemp` and
`g_lastAcceptedDesiredFromIsrC` are updated immediately from the stable
display value — making the confirmation poll succeed within the first few
polls instead of timing out.

The existing blink-based path (for spa models that do blank between setpoint
values, e.g. SJB-HS) is unchanged and still takes priority when
`isDisplayBlinking` is true.

**Affected model:** SB-H20 / SSP-H-20-1 / SB-B20 (and likely variants).

---

## [1.4.5-debug] – 2026-06-22

### Debug build — MQTT-based ISR stats (not for production)

---

## [1.4.4-debug] – 2026-06-21

### Debug build — ISR blink-detection diagnostics

Serial debug output (`SERIAL_DEBUG`) extended with per-poll ISR state snapshots
in `setDesiredWaterTempCelsius` / `confirmSetpointChange`. Logs:
- `isDisplayBlinking`, `stableDisplayBlankCount`, `latestBlinkingTemp`,
  `stableBlinkingWaterTempCount` on every confirm poll
- ISR state before each `changeWaterTemp` call
- `requestSetTempDisplay` result with frame counter

`Serial.setTxTimeoutMs(0)` added in `setup()` so USB CDC output is non-blocking
even without an active host connection.

**Do not use in production.** Verbose output (~40 lines per temperature step).

---

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
