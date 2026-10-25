#pragma once
#include <EspMqttBase.h>

class MqttLight : public EspMqttBase {
public:
    // An empty host leaves MQTT off entirely — no connection, no reconnect attempts.
    void setup(const char* deviceId, const char* host, uint16_t port,
               const char* mqttUser = "", const char* mqttPass = "");
    void publishState();

protected:
    void onConnected() override;
    void onMessage(const char* topic, const char* payload) override;

private:
    void publishDiscovery();

    String _topicState;
    String _topicCommand;
    String _topicDiscovery;
};

extern MqttLight Mqtt;
