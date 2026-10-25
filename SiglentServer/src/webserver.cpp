#include "webserver.h"

#include <ArduinoJson.h>
#include <AsyncWebSocket.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "state.h"

// State globals defined in main.cpp
extern bool    ledOn;
extern String  ledColor;
extern uint8_t ledBrightness;
extern void    commitState();

static AsyncWebSocket _ws("/ws");

void broadcastState() {
    _ws.textAll(stateToJson(ledOn, ledColor, ledBrightness));
}

// ── Body handler helpers ──────────────────────────────────────────────────────

static bool parseJsonBody(uint8_t* data, size_t len, size_t index, size_t total,
                          JsonDocument& doc, AsyncWebServerRequest* req) {
    if (index + len < total) return false;
    if (deserializeJson(doc, (char*)data, len) != DeserializationError::Ok) {
        req->send(400, "application/json", "{\"error\":\"invalid JSON\"}");
        return false;
    }
    return true;
}

// LEDs, WebSocket, MQTT and the NVS save follow in loop() via commitState().
static void commitAndRespond(AsyncWebServerRequest* req) {
    commitState();
    req->send(200, "application/json", stateToJson(ledOn, ledColor, ledBrightness));
}

// A POST without a body never reaches onBody — answer it here so it doesn't hang.
static void requireBody(AsyncWebServerRequest* req) {
    if (req->contentLength() == 0) {
        req->send(400, "application/json", "{\"error\":\"JSON body required\"}");
    }
}

// ── Body handlers ─────────────────────────────────────────────────────────────

static void onBody_power(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                         size_t index, size_t total) {
    JsonDocument doc;
    if (!parseJsonBody(data, len, index, total, doc, req)) return;
    if (!doc["on"].is<bool>()) {
        req->send(400, "application/json", "{\"error\":\"on required\"}");
        return;
    }
    ledOn = doc["on"].as<bool>();
    commitAndRespond(req);
}

static void onBody_color(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                         size_t index, size_t total) {
    JsonDocument doc;
    if (!parseJsonBody(data, len, index, total, doc, req)) return;
    String color = doc["color"] | "";
    if (!isValidHexColor(color)) {
        req->send(400, "application/json", "{\"error\":\"invalid color\"}");
        return;
    }
    color.toLowerCase();
    ledColor = color;
    commitAndRespond(req);
}

static void onBody_brightness(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                              size_t index, size_t total) {
    JsonDocument doc;
    if (!parseJsonBody(data, len, index, total, doc, req)) return;
    if (!doc["brightness"].is<int>() || doc["brightness"].as<int>() < 0 ||
        doc["brightness"].as<int>() > 255) {
        req->send(400, "application/json", "{\"error\":\"brightness 0-255 required\"}");
        return;
    }
    ledBrightness = doc["brightness"].as<int>();
    commitAndRespond(req);
}

// ── Setup / loop ──────────────────────────────────────────────────────────────
// /api/deviceinfo, /api/settings, /api/networkdata and /api/networkset come from
// EspProvision's common endpoints.

void webserverSetup(AsyncWebServer& server) {
    _ws.onEvent([](AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type,
                   void*, uint8_t*, size_t) {
        if (type == WS_EVT_CONNECT) client->text(stateToJson(ledOn, ledColor, ledBrightness));
    });
    server.addHandler(&_ws);

    server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "application/json", stateToJson(ledOn, ledColor, ledBrightness));
    });

    server.on("/api/power", HTTP_POST, requireBody, nullptr, onBody_power);

    server.on("/api/color", HTTP_POST, requireBody, nullptr, onBody_color);

    server.on("/api/brightness", HTTP_POST, requireBody, nullptr, onBody_brightness);

    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
}

void webserverLoop() {
    _ws.cleanupClients();
}
