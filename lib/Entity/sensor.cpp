#include "sensor.h"
#include "ArduinoJson.h"

void Sensor::publishValue() {
    client->publish(state_topic.c_str(), (*value).c_str(), (*retain));
}

void Sensor::mqttDiscovery(DevInfo* dev_info) {
    std::string home_topic = "homeassistant/sensor/" + dev_info->dev_name + "/" + name;
    std::string discovery_topic = home_topic + "/config";
    std::string txt;
    JsonDocument json;
    json["~"] = home_topic;
    json["name"] = friendly_name==""?name:friendly_name;
    json["obj_id"] = dev_info->dev_name + "_" + name;
    json["uniq_id"] = dev_info->dev_name + "_" + name;
    json["stat_t"] = "~"+stat_t;
    json["avty_t"] = dev_info->dev_name + "/status";
    if (ic != "") json["ic"] = ic;
    if (ent_cat != "") json["ent_cat"] = ent_cat;
    JsonObject dev = json["dev"].to<JsonObject>();
    dev["ids"] = dev_info->mqtt_entity_id;
    dev["name"] = dev_info->name;
    dev["sw"] = dev_info->firmware;
    dev["mdl"] = dev_info->product;
    dev["mf"] = dev_info->manufacturer;
    dev["cu"] = dev_info->control;
    serializeJson(json, txt);
    state_topic = home_topic + stat_t;
    client->beginPublish(discovery_topic.c_str(), txt.length(), false);
    client->print(txt.c_str());
    client->endPublish();
    vTaskDelay(pdMS_TO_TICKS(500));  
    publishValue();   
}