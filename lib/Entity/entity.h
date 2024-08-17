#pragma once
#include <string>
#include <PubSubClient.h>

struct DevInfo {
    std::string name;
    std::string manufacturer;
    std::string product;
    std::string firmware;
    std::string control;
    std::string mqtt_entity_id;
    std::string dev_name;
};

class Entity {
  public:
    std::string name;
    std::string friendly_name;
    std::string short_name;
    std::string ent_cat;
    std::string ic;
    std::string callback_topic;
    std::string state_topic;
    uint8_t category;
    bool * retain;
    PubSubClient* client;
    const std::string stat_t = "/state";
    const std::string cmd_t = "/set";
    Entity(std::string name, PubSubClient* client, bool * retain) {this->name = name; this->client = client; this->retain = retain;}
    virtual void callback(std::string  message) {}
    virtual void mqttDiscovery(DevInfo* dev_info) {}
};