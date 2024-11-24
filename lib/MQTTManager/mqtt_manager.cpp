#include "mqtt_manager.h"

static const char * TAG = "MQTT";

MQTTManager::MQTTManager(std::string server, uint16_t port, std::string login, std::string passwd) {
    this->server = server;
    this->port = port;
    this->login = login;
    this->passwd = passwd;
    this->mqtt_started = false;
    ns_client = new WiFiClient();
    mqtt_client = new PubSubClient(*ns_client);
}

MQTTManager::~MQTTManager() {
    mqtt_started = false;
    mqtt_client->disconnect();
    entities.clear();
    callback_entity.clear();
    delete mqtt_client; mqtt_client = nullptr;
    delete ns_client; ns_client = nullptr;
}

void MQTTManager::device_discovery() {
  for (Entity* entity : entities) {
    entity->mqttDiscovery(device_info);
    if (entity->callback_topic != "") callback_entity.insert(std::make_pair(entity->callback_topic, entity));
  }
}

void MQTTManager::device_online() {
    mqtt_client->publish((device_info->dev_name+"/status").c_str(), "online");
}

void MQTTManager::device_offline() {
    mqtt_client->publish((device_info->dev_name+"/status").c_str(), "offline");
}

void MQTTManager::handle() {
    if (!mqtt_started) return;
    if (!mqtt_client->connected()) {
        if (millis() - previousMQTTMillis >= 15000) {
            mqtt_client->disconnect();
            mqtt_client->setServer(server.c_str(), port);
            Serial.printf("[%s] Attempting MQTT connection: %s:%d\n", TAG, server.c_str(), port);
            if (mqtt_client->connect(client_id.c_str(), login.c_str(), passwd.c_str())) {
                previousMQTTMillis = millis();
                last_error = 0;
                device_discovery();
                device_online();
            } else {
                last_error = 5;
                Serial.printf("[%s] Connection error: %d\n", TAG, mqtt_client->state());
            }
            previousMQTTMillis = millis();
        }
    } else {
        mqtt_client->loop();
        if (millis() - previousMQTTMillis >= 15000) {
            previousMQTTMillis = millis();
            last_error = 0;
            device_online();
        }
    }
}