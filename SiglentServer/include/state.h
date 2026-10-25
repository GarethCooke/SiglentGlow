#pragma once

// Allow compiling on a native (non-Arduino) host for unit testing
#ifndef ARDUINO
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
using String = std::string;
#endif

#include <ArduinoJson.h>

inline int clampByte(int v) {
    return v < 0 ? 0 : (v > 255 ? 255 : v);
}

// Returns the REST / WebSocket state: {"on":true,"color":"#rrggbb","brightness":128}
inline String stateToJson(bool on, const String& color, uint8_t brightness) {
    JsonDocument doc;
    doc["on"]         = on;
    doc["color"]      = color;
    doc["brightness"] = brightness;
    String out;
    serializeJson(doc, out);
    return out;
}

// True for "#rrggbb" (hex digits in either case).
inline bool isValidHexColor(const String& color) {
    if (color.length() != 7 || color[0] != '#') return false;
    for (size_t i = 1; i < 7; i++) {
        if (!isxdigit((unsigned char)color[i])) return false;
    }
    return true;
}

// Converts RGB components (0-255) to a "#rrggbb" hex string.
// Values are clamped to [0, 255].
inline String rgbToHex(int r, int g, int b) {
    char hex[8];
    snprintf(hex, sizeof(hex), "#%02x%02x%02x", clampByte(r), clampByte(g), clampByte(b));
    return String(hex);
}

// Splits "#rrggbb" into components.
// Returns false (outputs untouched) if the colour is not valid.
inline bool hexToRgb(const String& color, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (!isValidHexColor(color)) return false;
    unsigned long c = strtoul(color.c_str() + 1, nullptr, 16);
    r = (c >> 16) & 0xFF;
    g = (c >> 8) & 0xFF;
    b = c & 0xFF;
    return true;
}

// Returns the Home Assistant JSON-schema state, which differs from the REST JSON:
// {"state":"ON","color_mode":"rgb","brightness":128,"color":{"r":255,"g":255,"b":255}}
inline String haStateToJson(bool on, const String& color, uint8_t brightness) {
    uint8_t r = 0, g = 0, b = 0;
    hexToRgb(color, r, g, b);
    JsonDocument doc;
    doc["state"]      = on ? "ON" : "OFF";
    doc["color_mode"] = "rgb";
    doc["brightness"] = brightness;
    JsonObject rgb    = doc["color"].to<JsonObject>();
    rgb["r"]          = r;
    rgb["g"]          = g;
    rgb["b"]          = b;
    String out;
    serializeJson(doc, out);
    return out;
}

// Parses a Home Assistant JSON-schema command such as
// {"state":"ON","color":{"r":255,"g":0,"b":0},"brightness":200} — every key optional.
// Populates out* only when those keys are present; brightness is clamped to [0, 255].
// Returns false if the payload is not a JSON object; true otherwise.
inline bool parseMqttCommand(const String& payload, bool& outOn, String& outColor,
                             uint8_t& outBrightness) {
    JsonDocument doc;
    if (deserializeJson(doc, payload) != DeserializationError::Ok) return false;
    if (!doc.is<JsonObject>()) return false;

    if (doc["state"].is<const char*>()) {
        String state = doc["state"].as<const char*>();
        if (state == "ON") outOn = true;
        if (state == "OFF") outOn = false;
    }
    JsonVariantConst color = doc["color"];
    if (color["r"].is<int>() && color["g"].is<int>() && color["b"].is<int>()) {
        outColor = rgbToHex(color["r"].as<int>(), color["g"].as<int>(), color["b"].as<int>());
    }
    if (doc["brightness"].is<float>()) {
        float brightness = doc["brightness"].as<float>();
        outBrightness    = brightness <= 0 ? 0 : brightness >= 255 ? 255 : (uint8_t)(brightness + 0.5f);
    }
    return true;
}
