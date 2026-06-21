# Setup Guide

This guide walks you through flashing the firmware, configuring the device,
and connecting it to Home Assistant.

---

## Prerequisites

- ESP32-S3 module wired to the Intex PureSpa control panel (see wiring below)
- A computer with a USB port
- A running MQTT broker (e.g. Mosquitto add-on in Home Assistant)
- Home Assistant with MQTT integration enabled

---

## Wiring

Connect the ESP32-S3 to the spa control panel bus and the on-board NTC thermistor:

| Signal | GPIO | Description                        |
|--------|------|------------------------------------|
| CLOCK  | 6    | Bus clock (input, rising-edge ISR) |
| DATA   | 7    | Bus data (input / open-drain out)  |
| LATCH  | 8    | Bus latch / frame sync (input)     |
| NTC    | 4    | NTC thermistor ADC input           |

Power the ESP32-S3 from the 5 V / VIN pin of the spa control panel.
The panel provides enough current reserve. A PTC fuse rated 1000 mA is
recommended on the supply line.

---

## Flashing the Firmware

### Option A — Fresh board (first-time flash via USB)

Use the merged binary which contains the bootloader, partition table, and
application in a single file starting at address `0x0`.

1. Connect the ESP32-S3 to your computer via USB.
2. Open [https://esp.huhn.me](https://esp.huhn.me) or use `esptool.py`.
3. Select the COM port and flash:

   **esptool.py:**
   ```
   esptool.py --chip esp32s3 --port COM3 write_flash 0x0 esp32-s3-intexsbh20-merged.bin
   ```

   **Web flasher (esp.huhn.me):**
   - Click **Add File**, set address to `0x0`, select `esp32-s3-intexsbh20-merged.bin`
   - Click **Program**

### Option B — OTA update (device already running)

1. Open the device web interface at `http://192.168.4.1` or the device IP.
2. Scroll to **OTA Update**.
3. Select `esp32-s3-intexsbh20.bin` (app binary, not merged).
4. Click **Upload & Flash**. The device reboots automatically.

---

## Arduino IDE Board Settings (compiling from source)

| Setting              | Value              |
|----------------------|--------------------|
| Board                | ESP32S3 Dev Module |
| PSRAM                | Disabled           |
| USB CDC On Boot      | Enabled            |
| Flash Size           | 8MB (or match your module) |
| Partition Scheme     | Default 4MB with spiffs (or larger) |

> **PSRAM must be Disabled.** Modules without PSRAM will fail to boot or
> log `E octal_psram: PSRAM chip is not connected` if compiled with PSRAM
> enabled.

After compiling: **Sketch → Export Compiled Binary** produces two files in
the sketch folder:
- `esp32-intexsbh20.ino.bin` — app only, use for OTA
- `esp32-intexsbh20.ino.merged.bin` — full image, use for first-time USB flash

---

## First-Time Configuration

1. Power on the ESP32-S3.
2. It creates a WiFi access point named **Intex_PureSpa**.
3. Connect to **Intex_PureSpa** from your phone or computer.
4. The setup page opens automatically (captive portal).
   If it does not open, navigate to `http://192.168.4.1` manually.
5. Fill in the settings:

   | Field             | Description                                      |
   |-------------------|--------------------------------------------------|
   | MQTT Device Name  | Name shown in Home Assistant (default: Intex_PureSpa) |
   | Wi-Fi SSID        | Your home network name                           |
   | Wi-Fi Password    | Your home network password                       |
   | MQTT Server       | IP address of your MQTT broker                   |
   | MQTT Port         | Default: 1883                                    |
   | MQTT User/Pass    | MQTT credentials (leave empty if not required)   |
   | Spa Model         | SB-H20 / SSP-H-20-1 / SB-B20 or SJB-HS          |

6. Click **Save & Restart**.
7. The device connects to your network. The AP closes automatically once
   the connection is established.

---

## Home Assistant Integration

The device uses MQTT autodiscovery. No manual configuration in Home Assistant
is required.

After the device connects to MQTT, the following entities appear automatically
under the configured device name:

| Entity          | Type    | Description                         |
|-----------------|---------|-------------------------------------|
| Climate         | climate | Temperature control (heat / off)    |
| Water Temp Act  | sensor  | Current water temperature           |
| Water Temp Set  | sensor  | Target water temperature            |
| Power           | switch  | Spa on/off                          |
| Filter          | switch  | Filtration on/off                   |
| Heater          | switch  | Heater on/off                       |
| Bubble          | switch  | Bubble jets on/off                  |
| Error           | sensor  | Panel error code                    |
| WiFi RSSI       | sensor  | Signal strength                     |
| Controller Temp | sensor  | ESP32-S3 board temperature          |
| Firmware        | sensor  | Firmware version                    |

### Climate entity behavior

- Setting mode to **heat** turns the spa on (if off) and enables heating.
- Setting mode to **off** disables heating only; the spa stays on.

---

## Resetting Credentials

If you need to move the device to a different network or change MQTT settings:

1. Connect to the **Intex_PureSpa** AP (the fallback AP is always available
   when the device cannot reach its configured WiFi network).
2. Open `http://192.168.4.1`.
3. Click **Reset Credentials** (red button).
4. Confirm the dialog. WiFi SSID/password and MQTT credentials are cleared.
   All other settings (spa model, language, port, etc.) are kept.
5. Reconfigure as described in the First-Time Configuration section.

---

## Serial Debugging

Connect via USB and open a serial monitor at **74880 baud**. On boot the
device prints:

```
ESP reset reason N
Intex_PureSpa MQTT WiFi Controller 1.4.2-esp32-s3
build with Arduino Core for ESP32 vX.X.X
PIN CLOCK=6 DATA=7 LATCH=8 NTC=4
Free heap: XXXXXX bytes
softAP start: OK (IP: 192.168.4.1)
```

Once the spa panel is detected:
```
[DBG Xs] Spa panel ONLINE! frames=N dropped=N
```

---

## Troubleshooting

**Setup page does not open automatically after connecting to Intex_PureSpa**
Navigate to `http://192.168.4.1` manually.

**Device appears as `esp32s3-XXXXXX` in the router**
Reflash with the latest firmware — the hostname fix is included from
v1.4.2-esp32-s3 onwards.

**Controller temperature shows an error or unrealistic value**
Check that the NTC thermistor is connected to GPIO4 and the jumper is seated.
Measure the voltage at GPIO4: it should be approximately 2.1–2.7 V at room
temperature.

**Spa panel is not detected (no ONLINE message)**
Verify wiring on CLOCK (GPIO6), DATA (GPIO7), and LATCH (GPIO8). Check that
3.3 V is supplied to the PCB — the DATA line pull-up resistor (R3) requires
this voltage.

**`E octal_psram: PSRAM chip is not connected` on boot**
Set PSRAM to **Disabled** in Arduino IDE board settings and recompile.

**MQTT entities do not appear in Home Assistant**
Confirm the MQTT broker is reachable, the MQTT discovery prefix matches the
setting in Home Assistant (default: `homeassistant`), and the MQTT mode is
set to **HA discovery**.
