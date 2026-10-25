// SiglentGlow — XIAO ESP32-S3 backlight controller for the Siglent bench-instrument tower
// Hardware: Seeed XIAO ESP32-S3, 74AHCT125 level shifter, two WS2812B strips + two 5 mm
// WS2812 sill pixels, 5 V from the bench USB strip (XIAO fed through a Schottky diode).
// See C:\Development\3DPrinting\SiglentTower (parts/wiring/wiring-4-connections.png).
//
// PIN MAPPING (see config.h)
//   PIN_BUS1   GPIO1  D0  RMT    sill 1 → sill 2 → left strip  (indices 0-1 sill, 2-16 left)
//   PIN_BUS2   GPIO2  D1  RMT    right strip                    (indices 17-31)
//   PIN_BUTTON GPIO5  D4  Input  INPUT_PULLUP · short press = on/off · hold 5 s = factory reset

#include <Arduino.h>
#include <ButtonStatus.h>
#include <Preferences.h>
#include <TwoStateValue.h>
#include <atomic>

#include <EspDevice.h>
#include <EspProvision.h>

#include "config.h"
#include "leds.h"
#include "mqtt.h"
#include "state.h"
#include "webserver.h"

#define COUNTDOWN_START_MS 1000  // hold this long before the reset countdown shows
#define HOLD_DURATION_MS   5000
#define FLASH_COUNT        5
#define FLASH_ON_MS        150
#define FLASH_OFF_MS       150

static constexpr char NVS_KEY_ON[]         = "on";
static constexpr char NVS_KEY_COLOR[]      = "color";
static constexpr char NVS_KEY_BRIGHTNESS[] = "brightness";

// ── Light state — referenced as extern by webserver.cpp and mqtt.cpp ─────────
bool    ledOn         = DEFAULT_ON;
String  ledColor      = DEFAULT_COLOR;
uint8_t ledBrightness = DEFAULT_BRIGHTNESS;

ButtonStatus* pBtn = nullptr;

static std::atomic<bool> statePending{false};  // set by commitState(), handled in loop()
static bool              savePending = false;
static uint32_t          saveDueAt   = 0;
static String            savedState;           // last state written to NVS, as JSON

// ── State persistence ─────────────────────────────────────────────────────────

static void loadState() {
    Preferences prefs;
    prefs.begin(STATE_NVS_NAMESPACE, /*readOnly=*/true);
    ledOn         = prefs.getBool(NVS_KEY_ON, DEFAULT_ON);
    ledColor      = prefs.getString(NVS_KEY_COLOR, DEFAULT_COLOR);
    ledBrightness = prefs.getUChar(NVS_KEY_BRIGHTNESS, DEFAULT_BRIGHTNESS);
    prefs.end();
    if (!isValidHexColor(ledColor)) ledColor = DEFAULT_COLOR;
    savedState = stateToJson(ledOn, ledColor, ledBrightness);
}

// Skips the write when the state is back where it was (e.g. toggled twice).
static void saveState() {
    String current = stateToJson(ledOn, ledColor, ledBrightness);
    if (current == savedState) return;
    Preferences prefs;
    prefs.begin(STATE_NVS_NAMESPACE, /*readOnly=*/false);
    prefs.putBool(NVS_KEY_ON, ledOn);
    prefs.putString(NVS_KEY_COLOR, ledColor);
    prefs.putUChar(NVS_KEY_BRIGHTNESS, ledBrightness);
    prefs.end();
    savedState = current;
}

// ── State changes ─────────────────────────────────────────────────────────────

static void applyLedState() {
    uint8_t r = 255, g = 255, b = 255;
    hexToRgb(ledColor, r, g, b);
    ledsSetState(ledOn, CRGB(r, g, b), ledBrightness);
}

// Single path for every state change (REST, MQTT, button). REST handlers run on the
// AsyncTCP task, so the fan-out happens in loop() where FastLED and PubSubClient live.
void commitState() {
    statePending = true;
}

static void handleStateChange(uint32_t now) {
    if (!statePending.exchange(false)) return;
    applyLedState();
    broadcastState();
    Mqtt.publishState();
    savePending = true;
    saveDueAt   = now + STATE_SAVE_DELAY_MS;  // each change pushes the save back
}

static void handleStateSave(uint32_t now) {
    if (savePending && (int32_t)(now - saveDueAt) >= 0) {
        savePending = false;
        saveState();
    }
}

// ── Button: short press toggles power, hold 1-5 s counts down to factory reset ─
// Returns true once a factory reset is under way.

static bool handleButton(uint32_t now) {
    static bool     btnDown      = false;
    static uint32_t btnDownStart = 0;

    ButtonStatus::button_state btn = pBtn->getState();
    if (!ButtonStatus::isKnownState(btn)) return false;  // still bouncing

    if (ButtonStatus::isOn(btn)) {
        if (!btnDown) {
            btnDown      = true;
            btnDownStart = now;
        }
        uint32_t held = now - btnDownStart;
        if (held < COUNTDOWN_START_MS) return false;

        // Blink faster as the hold approaches 5 s
        uint32_t blinkRate = map(constrain(held, COUNTDOWN_START_MS, HOLD_DURATION_MS),
                                 COUNTDOWN_START_MS, HOLD_DURATION_MS, 800, 80);
        ledsSetResetCountdown(true, ((held - COUNTDOWN_START_MS) / blinkRate) % 2 == 0);
        if (held >= HOLD_DURATION_MS) {
            ledsBlinkSill(CRGB::Red, FLASH_COUNT, FLASH_ON_MS, FLASH_OFF_MS);
            ledsOff();
            Provision.factoryReset();
            return true;
        }
    } else if (btnDown) {
        btnDown = false;
        ledsSetResetCountdown(false, false);
        // Released before the countdown: toggle. Released during it: cancel, change nothing.
        // In setup mode the strips stay dark, so there is nothing to toggle.
        if (now - btnDownStart < COUNTDOWN_START_MS && !Provision.isApMode()) {
            ledOn = !ledOn;
            commitState();
        }
    }
    return false;
}

// ── Arduino lifecycle ─────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    ledsSetup();
    loadState();

    pBtn = new ButtonStatus(
        make_shared_ptr_lite<TwoStateValue>(new DigitalPinValue(PIN_BUTTON, false)));
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    Provision.onStation([](AsyncWebServer& srv) {
        webserverSetup(srv);
        String mqttHost = Provision.getMqttHost(MQTT_HOST);
        String mqttUser = Provision.getMqttUser();
        String mqttPass = Provision.getMqttPass();
        Mqtt.setup(EspDevice::id().c_str(), mqttHost.c_str(), Provision.getMqttPort(MQTT_PORT),
                   mqttUser.c_str(), mqttPass.c_str());
    });

    EspProvisionConfig cfg;
    cfg.apPassword       = "siglentglow";
    cfg.nvsNamespace     = "siglentglow";
    cfg.otaPassword      = OTA_PASSWORD;
    cfg.friendlyHostname = "siglentglow";
    cfg.serviceType      = "siglentglow";
    Provision.begin("siglentglow", "SiglentGlow", cfg);

    ledsBlinkSill(CRGB::White, 2, 200, 200);
    ledsSetSetupMode(Provision.isApMode());
    applyLedState();
}

void loop() {
    Provision.loop();

    if (!Provision.isApMode()) {
        webserverLoop();
        Mqtt.loop();
    }

    uint32_t now = millis();
    if (handleButton(now)) return;  // factory reset pending — Provision.loop() restarts next pass
    handleStateChange(now);
    handleStateSave(now);
    ledsLoop(now);
}
