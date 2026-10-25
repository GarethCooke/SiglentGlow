#include "leds.h"
#include "config.h"

#define FRAME_MS        20  // redraw cap (~50 fps)
#define SETUP_PULSE_BPM 30  // 2 s breathing cycle
#define SETUP_PULSE_MIN 8

static const CRGB AMBER(255, 100, 0);

static CRGB _leds[LEDS_TOTAL];
static CRGB _shown[LEDS_TOTAL];  // last frame sent to the strips

static bool     _on          = false;
static CRGB     _colour      = CRGB::Black;
static uint8_t  _brightness  = 0;
static bool     _setupMode   = false;
static bool     _resetActive = false;
static bool     _resetLit    = false;
static uint32_t _lastFrame   = 0;

// ── Helpers ───────────────────────────────────────────────────────────────────

static CRGB scaled(CRGB colour, uint8_t level) {
    colour.nscale8_video(level);
    return colour;
}

static void fillSill(const CRGB& colour) {
    fill_solid(_leds + SILL_FIRST, LEDS_SILL, colour);
}

static void show() {
    FastLED.show();
    memcpy(_shown, _leds, sizeof(_leds));
}

// ── Public API ────────────────────────────────────────────────────────────────

void ledsSetup() {
    FastLED.addLeds<WS2812B, PIN_BUS1, GRB>(_leds, LEDS_BUS1);
    FastLED.addLeds<WS2812B, PIN_BUS2, GRB>(_leds + LEDS_BUS1, LEDS_BUS2);
    FastLED.setMaxPowerInVoltsAndMilliamps(LED_VOLTS, LED_MAX_MA);
    // Brightness is applied per pixel (so status colours keep their own level) and
    // frames are only sent on change, so temporal dithering would just freeze mid-step.
    FastLED.setDither(DISABLE_DITHER);
    ledsOff();
}

void ledsSetState(bool on, const CRGB& colour, uint8_t brightness) {
    _on         = on;
    _colour     = colour;
    _brightness = brightness;
}

void ledsSetSetupMode(bool setupMode) {
    _setupMode = setupMode;
}

void ledsSetResetCountdown(bool active, bool lit) {
    _resetActive = active;
    _resetLit    = lit;
}

void ledsBlinkSill(const CRGB& colour, int count, uint32_t onMs, uint32_t offMs) {
    for (int i = 0; i < count; i++) {
        fillSill(scaled(colour, STATUS_BRIGHTNESS)); show(); delay(onMs);
        fillSill(CRGB::Black);                       show(); delay(offMs);
    }
}

void ledsOff() {
    fill_solid(_leds, LEDS_TOTAL, CRGB::Black);
    show();
}

void ledsLoop(uint32_t now) {
    if (now - _lastFrame < FRAME_MS) return;
    _lastFrame = now;

    // Strips: the light state; dark when off or in setup mode
    CRGB strip = (_on && !_setupMode) ? scaled(_colour, _brightness) : CRGB(CRGB::Black);
    fill_solid(_leds + LEFT_FIRST, LEDS_LEFT, strip);
    fill_solid(_leds + RIGHT_FIRST, LEDS_RIGHT, strip);

    // Sill: follows the strips unless a status takes over (highest priority first)
    CRGB sill = strip;
    if (_resetActive) {
        sill = _resetLit ? scaled(CRGB::Red, STATUS_BRIGHTNESS) : CRGB(CRGB::Black);
    } else if (_setupMode) {
        sill = scaled(AMBER, beatsin8(SETUP_PULSE_BPM, SETUP_PULSE_MIN, STATUS_BRIGHTNESS));
    }
    fillSill(sill);

    if (memcmp(_leds, _shown, sizeof(_leds)) != 0) show();
}
