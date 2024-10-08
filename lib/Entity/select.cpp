#include "select.h"
#include "ArduinoJson.h"

void Select::publishValue() {
    client->publish(state_topic.c_str(), items_list[*value].c_str(), (*retain));
}

Select::~Select(){
    items_list.clear();
}

void Select::callback(std::string message) {
    for (auto& item: items_list) {
        if (item.second == message) {
            *value = item.first;
            break;
        }
    }
}

std::string Select::getItemsString() {
    std::string result = "";
    for (auto& item: items_list) result = result + item.second + ",";
    if (result.length() > 0 ) result.erase(result.length()-1, 1);
    return result;
}

void Select::mqttDiscovery(DevInfo* dev_info) {
    std::string home_topic = "homeassistant/select/" + dev_info->dev_name + "/" + name;
    std::string discovery_topic = home_topic + "/config";
    std::string txt;
    JsonDocument json;
    json["~"] = home_topic;
    json["name"] = friendly_name==""?name:friendly_name;
    json["obj_id"] = dev_info->dev_name + "_" + name;
    json["uniq_id"] = dev_info->dev_name + "_" + name;
    json["stat_t"] = "~"+stat_t;
    json["cmd_t"] = "~"+cmd_t;
    json["avty_t"] = dev_info->dev_name + "/status";
    if (ic != "") json["ic"] = ic;
    if (ent_cat != "") json["ent_cat"] = ent_cat;
    JsonArray ops = json["ops"].to<JsonArray>();
    for (auto& item: items_list) ops.add(item.second);
    JsonObject dev = json["dev"].to<JsonObject>();
    dev["ids"] = dev_info->mqtt_entity_id;
    dev["name"] = dev_info->name;
    dev["sw"] = dev_info->firmware;
    dev["mdl"] = dev_info->product;
    dev["mf"] = dev_info->manufacturer;
    dev["cu"] = dev_info->control;
    serializeJson(json, txt);
    callback_topic = home_topic + cmd_t;
    state_topic = home_topic + stat_t;
    client->beginPublish(discovery_topic.c_str(), txt.length(), (*retain));
    client->print(txt.c_str());
    client->endPublish();
    client->subscribe(callback_topic.c_str());
    vTaskDelay(pdMS_TO_TICKS(500));  
    publishValue();
}