# SiglentGlow

Backlight controller for the Siglent bench-instrument tower: ESP32 firmware + web UI with Home Assistant integration. A sibling of MorayGlow, built on the same IguanaPlatform libraries.

## What it does

- Drives two WS2812B strips (left and right) and two 5 mm WS2812 sill pixels, all showing one static colour at one brightness
- Web UI at `http://siglentglow.local`: power, colour, brightness
- Appears in Home Assistant as an RGB light with brightness (MQTT auto-discovery)
- Remembers on/off, colour and brightness across power cycles
- Button: short press toggles on/off; hold 5 s to factory-reset Wi-Fi (the sill pixels blink red faster and faster)
- Sill pixels as status lights: amber breathing means setup mode; two white blinks mean booting

## Hardware

| Item | Detail |
|---|---|
| MCU | Seeed XIAO ESP32-S3 |
| Bus 1 | D0 (GPIO1) → 74AHCT125 → 330 Ω → sill 1 → sill 2 → left strip (17 LEDs) |
| Bus 2 | D1 (GPIO2) → 74AHCT125 → 330 Ω → right strip (15 LEDs) |
| Button | D4 (GPIO5) to GND, internal pull-up |
| Power | 5 V USB; LEDs fed directly, XIAO 5V through a Schottky diode; LED budget 1500 mA (enforced in firmware) |

The wiring sheet and enclosure are in `C:\Development\3DPrinting\SiglentTower`. LED counts live in `SiglentServer/include/config.h`.

## Project Structure

```text
SiglentGlow/
└── SiglentServer/          # ESP32 firmware (C++, PlatformIO)
    ├── platformio.ini
    ├── include/            # config.h, state.h, leds.h, mqtt.h, webserver.h
    ├── src/                # main.cpp, leds.cpp, mqtt.cpp, webserver.cpp
    ├── data/               # web UI (LittleFS): index, setup, settings, css/, js/
    └── test/test_state.cpp # Unity tests (run on host)
```

## Flashing

PlatformIO's installer doesn't put `pio` on PATH on Windows, so the commands below start by aliasing it for the current PowerShell session. Skip that line if `%USERPROFILE%\.platformio\penv\Scripts` is already on your PATH.

```powershell
Set-Alias pio "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
cd SiglentServer
pio run -e xiao_esp32s3 -t upload     # firmware over USB
pio run -e xiao_esp32s3 -t uploadfs   # web UI over USB
```

USB upload and the serial monitor are pinned to `COM5` in `platformio.ini`. If the board shows up on a different port, change `upload_port` / `monitor_port` there, or override the port for one run with `--upload-port COMx`.

After the first setup, use the `xiao_esp32s3_ota` environment for Wi-Fi updates (target `siglentglow.local`, password `password`).

## Setting it up

1. Power it up. The sill pixels breathe amber (setup mode).
2. Join the Wi-Fi network `SiglentGlow-XXXXXX` with the password `siglentglow`. The setup page opens (or browse to `http://10.0.0.1`).
3. Pick your Wi-Fi network, enter its password, and press **Connect**. The device restarts.
4. Open `http://siglentglow.local`.
5. For Home Assistant, open **Device settings** (`/settings.html`), enter the MQTT broker host, port, user and password, and save. The device restarts and appears in HA as **SiglentGlow**.

MQTT stays off until a broker host is set.

## Tests

```powershell
Set-Alias pio "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
cd SiglentServer
pio test -e native
```

These need `gcc`/`g++` on PATH (MinGW-w64).
