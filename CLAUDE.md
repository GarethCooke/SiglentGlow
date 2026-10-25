# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

SiglentGlow is the backlight controller in the Siglent bench-instrument tower. It drives two short WS2812B strips and two 5 mm WS2812 "sill" pixels from a Seeed XIAO ESP32-S3. It is a sibling of **MorayGlow**: same architecture, same shared libraries, same conventions.

- **SiglentServer/**: ESP32-S3 firmware (C++, PlatformIO, Arduino framework) plus the web UI it serves from LittleFS (`data/`)

Hardware design (enclosure, wiring sheet `parts/wiring/wiring-4-connections.png`) lives in `C:\Development\3DPrinting\SiglentTower`.

Shared code comes from sibling folders and **must not be modified from this project**:

- `../IguanaPlatform`: `EspProvision`, `EspDevice`, `EspMqttBase`, `EspLogger`, `scripts/gen_build_info.py`
- `../IguanaCommon/IguanaLib`: `ButtonStatus`, `TwoStateValue`

## Build & Flash Commands (Firmware)

`pio` is not on PATH on Windows. Always invoke as:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" <args>
```

Run from `SiglentServer/`:

```bash
# Build firmware
pio run -e xiao_esp32s3

# Upload firmware to device (USB)
pio run -e xiao_esp32s3 -t upload

# Build / upload web UI files to LittleFS (USB)
pio run -e xiao_esp32s3 -t buildfs
pio run -e xiao_esp32s3 -t uploadfs

# Upload firmware / web UI over Wi-Fi (OTA, default target siglentglow.local)
pio run -e xiao_esp32s3_ota -t upload
pio run -e xiao_esp32s3_ota -t uploadfs

# Serial monitor
pio device monitor

# Run native C++ unit tests (no device needed; needs gcc/g++ on PATH)
pio test -e native
```

### OTA Notes

- OTA uses ArduinoOTA via the `xiao_esp32s3_ota` environment. Password: `password` on first boot (`OTA_PASSWORD`), changeable on `/settings.html`. If you change it, update `upload_flags` too.
- The mDNS hostname is `siglentglow.local` (`friendlyHostname`, single-device project).
- OTA needs the ESP32 to connect back to the host on an ephemeral port. Windows Firewall must allow inbound connections for `%USERPROFILE%\.platformio\penv\Scripts\python.exe`.
- IoT network isolation (common on mesh routers) blocks OTA. Use USB, or temporarily move the device to the main network.

## Architecture

### Data & Control Flow

```
Home Assistant
    ↕ MQTT (<device-id>/state, <device-id>/command)
ESP32-S3 Firmware (SiglentServer)
    ↕ REST + WebSocket (/api/state, /api/power, /api/color, /api/brightness, /ws)
Web Browser (data/)
```

Every state change, whatever its source (REST, MQTT or the button), sets the globals and calls `commitState()`. That only raises a flag. `loop()` then runs the fan-out on the loop task: `applyLedState()` → `broadcastState()` → `Mqtt.publishState()` → schedule the NVS save. REST handlers run on the AsyncTCP task, so deferring the fan-out keeps every `FastLED.show()` and PubSubClient call on one task.

### Key Files

- **[include/config.h](SiglentServer/include/config.h)**: pins, **LED counts** (the only place to change them; bus split and index ranges are derived), current limit, status brightness, NVS namespace and first-boot defaults, MQTT fallback host/port, OTA default.
- **[include/state.h](SiglentServer/include/state.h)**: pure state logic: `stateToJson`, `haStateToJson`, `rgbToHex`, `hexToRgb`, `isValidHexColor`, `parseMqttCommand`. Header-only with `inline` functions and `#ifndef ARDUINO` guards so it compiles natively for unit tests.
- **[include/leds.h](SiglentServer/include/leds.h)** / **[src/leds.cpp](SiglentServer/src/leds.cpp)**: FastLED on RMT, two outputs. Holds the light state plus the sill status overlays, and redraws at most every 20 ms, only when the frame changed. Loop task only.
- **[src/main.cpp](SiglentServer/src/main.cpp)**: globals, NVS persistence, `commitState()` and the fan-out, button handling, `setup()` / `loop()`.
- **[src/webserver.cpp](SiglentServer/src/webserver.cpp)**: light REST routes and `/ws`. Device-info, settings and Wi-Fi routes come from EspProvision's common endpoints; don't duplicate them.
- **[src/mqtt.cpp](SiglentServer/src/mqtt.cpp)**: `MqttLight : EspMqttBase`, HA discovery, command handling.
- **[data/js/siglent.js](SiglentServer/data/js/siglent.js)**: UMD utility module (browser + Node): `buildWsUrl`, `isValidHexColor`, `brightnessPercent`, `throttle`.

### Globals (main.cpp, extern'd into webserver.cpp and mqtt.cpp)

```cpp
bool    ledOn         = DEFAULT_ON;          // power state
String  ledColor      = DEFAULT_COLOR;       // "#rrggbb", lowercase
uint8_t ledBrightness = DEFAULT_BRIGHTNESS;  // 0-255
void    commitState();                       // call after changing any of the above
```

### LED Hardware

| Bus | Pin | LEDs | Indices |
|-----|-----|------|---------|
| 1 | GPIO1 (D0) → 74AHCT125 → 330 Ω | sill 1, sill 2, left strip | 0–1 sill, 2–16 left |
| 2 | GPIO2 (D1) → 74AHCT125 → 330 Ω | right strip | 17–31 |

- WS2812B, GRB, 5 V. One `CRGB` array of `LEDS_BUS1 + LEDS_BUS2`, two `addLeds` calls.
- `PIN_BUTTON` GPIO5 (D4): momentary to GND, `INPUT_PULLUP`, debounced by `ButtonStatus`.
- Avoid GPIO3 (strapping pin), GPIO0, GPIO45 and GPIO46.
- Current limit: `FastLED.setMaxPowerInVoltsAndMilliamps(5, 1500)`. User brightness is applied per pixel (`nscale8_video`) and the limiter caps the result, so status colours keep their own level (`STATUS_BRIGHTNESS`).
- Library: **FastLED 3.6.0** (pinned). It builds against Arduino-ESP32 2.0.14 (espressif32 6.5.0, IDF 4.4) and uses FastLED's own RMT driver.

### Behaviour

- **Light state**: on/off, colour, brightness. The strips and sill pixels show it; all dark when off.
- **Persistence**: on/colour/brightness are stored in NVS namespace `siglentstate`, written once 5 s after the last change, and skipped if nothing differs. First boot: on, `#ffffff`, 128.
- **Sill status**, highest priority first:
  1. Factory-reset countdown: button held ≥ 1 s. The sill blinks red, 800 → 80 ms per half-cycle as the hold nears 5 s. At 5 s: 5 red flashes, all dark, `Provision.factoryReset()`.
  2. Setup (AP) mode: the sill breathes amber (2 s cycle), strips dark.
  3. Boot: two white blinks (200/200 ms) before the state is applied.
- **Button**: a short press (< 1 s) toggles on/off (ignored in setup mode). Releasing during the countdown cancels it.

### REST API

| Method | Path | Body | Response |
|--------|------|------|----------|
| GET | `/api/state` | — | `{"on":bool,"color":"#rrggbb","brightness":0-255}` |
| POST | `/api/power` | `{"on":bool}` | state JSON |
| POST | `/api/color` | `{"color":"#rrggbb"}` | state JSON (400 if not a valid hex colour) |
| POST | `/api/brightness` | `{"brightness":0-255}` | state JSON (400 if missing or out of range) |
| WS | `/ws` | — | state JSON on connect and on every change |

From EspProvision: `GET /api/deviceinfo`, `GET/POST /api/settings`, `GET /api/networkdata`, `POST /api/networkset`, `GET /api/devices`.

### MQTT / Home Assistant

- The broker host/port come from NVS (`Provision.getMqttHost(MQTT_HOST)` / `getMqttPort(MQTT_PORT)`), and user/password from `getMqttUser()` / `getMqttPass()`. All are set on `/settings.html`, and saving restarts the device.
- **Empty host → MQTT off** (`suspend(true)`, no reconnect loop). `MQTT_HOST` defaults to `""`.
- Topics: `<device-id>/state` (retained) and `<device-id>/command`. Discovery goes to `homeassistant/light/<device-id>/config` (retained) and is republished on every reconnect.

The MQTT payloads use Home Assistant's JSON schema, which **differs from the REST API**:

```json
// command (subscribe): every key optional
{ "state": "ON", "color": { "r": 255, "g": 0, "b": 0 }, "brightness": 200 }

// state (publish, retained)
{ "state": "ON", "color_mode": "rgb", "brightness": 200, "color": { "r": 255, "g": 0, "b": 0 } }
```

### Device Identity

- Device ID: `siglentglow-xxxxxx` (last 3 MAC bytes, lowercase)
- AP SSID: `SiglentGlow-XXXXXX` (uppercase), password `siglentglow`, IP `10.0.0.1`, captive portal → `setup.html`
- mDNS: `siglentglow.local`, plus the `_siglentglow._tcp` service

### LittleFS

`data/` is uploaded with `uploadfs`. No build step: edit and re-upload.

- `index.html`: power button, LED-strip colour picker, brightness slider (≤ 1 POST per 200 ms while dragging), status line, theme toggle
- `setup.html`, `settings.html`: from IguanaPlatform's `data/`. `settings.html` adds the broker host/port fields.
- `css/style.css`: **the one stylesheet**, linked as `/css/style.css` by every page. IguanaPlatform base + MorayGlow control-page styles scoped to `.container`.
- `js/app.js`, `js/siglent.js`

## Library Conventions

- **ArduinoJson v7**: Use `JsonDocument` with no capacity argument (v7 is dynamic).
- **PubSubClient**: Always `setBufferSize(512)` (EspMqttBase does this). The default is too small for HA discovery payloads.
- **ESPAsyncWebServer**: POST bodies arrive in the `onBody` lambda (3rd arg to `server.on`), not the request handler.
- **WiFi scan**: async only (`WiFi.scanNetworks(true)` + `WiFi.scanComplete()`). EspProvision owns the scan; never block in a handler.
- **FastLED**: only call it from the loop task (via `leds.cpp`).
