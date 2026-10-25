#include "mqtt.h"
#include "config.h"
#include "state.h"
#include <ArduinoJson.h>
#include <EspDevice.h>
#include <EspLogger.h>

// State globals defined in main.cpp
extern bool    ledOn;
extern String  ledColor;
extern uint8_t ledBrightness;
extern void    commitState();

MqttLight Mqtt;

void MqttLight::setup(const char* deviceId, const char* host, uint16_t port,
                      const char* mqttUser, const char* mqttPass) {
    _topicState     = String(deviceId) + "/state";
    _topicCommand   = String(deviceId) + "/command";
    _topicDiscovery = String(MQTT_TOPIC_DISCOVERY_PREFIX) + "/light/" + deviceId + "/config";

    if (strlen(host) == 0) {
        LOG("[MQTT] No broker host set — MQTT off (set one on /settings.html)");
        suspend(true);
        return;
    }

    EspMqttBase::Config cfg;
    cfg.host     = host;
    cfg.port     = port;
    cfg.user     = mqttUser;
    cfg.password = mqttPass;
    EspMqttBase::setup(cfg);
}

void MqttLight::publishDiscovery() {
    JsonDocument doc;
    doc["name"]             = nullptr;  // null → the entity takes the device name
    doc["unique_id"]        = EspDevice::id();
    doc["state_topic"]      = _topicState;
    doc["command_topic"]    = _topicCommand;
    doc["schema"]           = "json";
    doc["brightness"]       = true;
    doc["brightness_scale"] = 255;

    JsonArray modes = doc["supported_color_modes"].to<JsonArray>();
    modes.add("rgb");

    JsonObject device        = doc["device"].to<JsonObject>();
    device["identifiers"][0] = EspDevice::id();
    device["name"]           = DEVICE_NAME;
    device["model"]          = "XIAO ESP32-S3";
    device["manufacturer"]   = "IguanaWare";

    String payload;
    serializeJson(doc, payload);
    publish(_topicDiscovery.c_str(), payload.c_str(), /*retain=*/true);
}

void MqttLight::onConnected() {
    subscribe(_topicCommand.c_str());
    publishDiscovery();
    publishState();
}

void MqttLight::onMessage(const char* topic, const char* payload) {
    bool    newOn         = ledOn;
    String  newColor      = ledColor;
    uint8_t newBrightness = ledBrightness;
    if (!parseMqttCommand(payload, newOn, newColor, newBrightness)) return;
    ledOn         = newOn;
    ledColor      = newColor;
    ledBrightness = newBrightness;
    commitState();
}

// HA JSON-schema state (not the REST JSON) — HA's json light only reads this shape.
void MqttLight::publishState() {
    if (isConnected()) {
        publish(_topicState.c_str(), haStateToJson(ledOn, ledColor, ledBrightness).c_str(),
                /*retain=*/true);
    }
}
