#pragma once
#define FASTLED_INTERNAL  // only silences FastLED's #pragma messages (SPI notes; we're clockless)
#include <FastLED.h>

// WS2812B output on the RMT peripheral, two buses:
//   bus 1 (PIN_BUS1) = sill pixels + left strip, bus 2 (PIN_BUS2) = right strip.
// Call these from the loop task only — FastLED.show() is not re-entrant.

void ledsSetup();

// The light state: the strips and sill pixels show this unless a status takes over.
void ledsSetState(bool on, const CRGB& colour, uint8_t brightness);

// Setup (AP) mode: strips dark, sill pixels breathe amber.
void ledsSetSetupMode(bool setupMode);

// Factory-reset countdown: sill pixels red (lit) or dark, overriding everything else.
void ledsSetResetCountdown(bool active, bool lit);

// Blocking sill blink (boot and factory-reset flashes); the strips are left as they are.
void ledsBlinkSill(const CRGB& colour, int count, uint32_t onMs, uint32_t offMs);

// All LEDs dark, immediately.
void ledsOff();

// Redraws at most every 20 ms and only sends a frame when it changed. Call every loop().
void ledsLoop(uint32_t now);
