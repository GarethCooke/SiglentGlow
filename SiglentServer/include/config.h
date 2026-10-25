#pragma once

// ── Pins (Seeed XIAO ESP32-S3) ────────────────────────────────────────────────
// GPIO3 is a strapping pin; GPIO0, GPIO45 and GPIO46 are avoided as in MorayGlow.
#define PIN_BUS1   1  // D0 → 74AHCT125 → 330Ω → sill 1 → sill 2 → left strip
#define PIN_BUS2   2  // D1 → 74AHCT125 → 330Ω → right strip
#define PIN_BUTTON 5  // D4 → momentary to GND, INPUT_PULLUP

// ── LED counts — the only place to change when a strip is swapped ─────────────
#define LEDS_SILL  2   // 5 mm WS2812 sill pixels, first on bus 1
#define LEDS_LEFT  15  // left strip, after the sill pixels on bus 1
#define LEDS_RIGHT 15  // right strip, alone on bus 2

// Derived: one CRGB array, bus 1 then bus 2
#define LEDS_BUS1   (LEDS_SILL + LEDS_LEFT)
#define LEDS_BUS2   (LEDS_RIGHT)
#define LEDS_TOTAL  (LEDS_BUS1 + LEDS_BUS2)
#define SILL_FIRST  0
#define LEFT_FIRST  (SILL_FIRST + LEDS_SILL)
#define RIGHT_FIRST (LEDS_BUS1)

// ── Power ─────────────────────────────────────────────────────────────────────
// FastLED's limiter caps total draw on top of the user's brightness.
#define LED_VOLTS  5
#define LED_MAX_MA 1500

// Sill pixel level for boot, setup and reset status (independent of user brightness)
#define STATUS_BRIGHTNESS 96

// ── Light state persistence ───────────────────────────────────────────────────
// Own namespace — EspProvision owns "siglentglow".
#define STATE_NVS_NAMESPACE "siglentstate"
#define STATE_SAVE_DELAY_MS 5000  // write once, this long after the last change

// First-boot defaults
#define DEFAULT_ON         true
#define DEFAULT_COLOR      "#ffffff"
#define DEFAULT_BRIGHTNESS 128

// ── MQTT ──────────────────────────────────────────────────────────────────────
// Fallback broker only — normally set at runtime on /settings.html (stored in NVS).
// An empty host leaves MQTT off until a broker is configured.
#define MQTT_HOST ""
#define MQTT_PORT 1883

// Device identity — AP SSID and MQTT topics are built at runtime from the MAC
// address (see EspDevice).
#define DEVICE_NAME "SiglentGlow"

// OTA update password — default on first boot; override at runtime via /settings.html.
#define OTA_PASSWORD "password"

// MQTT auto-discovery prefix (Home Assistant default)
#define MQTT_TOPIC_DISCOVERY_PREFIX "homeassistant"
