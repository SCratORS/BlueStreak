#include "settings_manager.h"
#include <ArduinoJson.h>

static const char* TAG = "SETTING";

SettingsManager::SettingsManager(std::string filename) {
    this->filename = filename;
    last_error= 0;
}

void SettingsManager::CheckDefault() {
    if (settings.wifi_ssid == "null") settings.wifi_ssid = "";
    if (settings.wifi_passwd == "null") settings.wifi_passwd = "";
    if (settings.mqtt_server == "null") settings.mqtt_server = "";
    if (settings.mqtt_login == "null") settings.mqtt_login = "";
    if (settings.mqtt_passwd == "null") settings.mqtt_passwd = "";
    if (settings.tlg_token == "null") settings.tlg_token = "";
    if (settings.tlg_user == "null") settings.tlg_user = "";
    if (settings.access_code == "null") settings.access_code = "";
    if (settings.user_login == "null") settings.user_login = "";
    if (settings.user_passwd == "null") settings.user_passwd = "";
}

void SettingsManager::LoadSettings(fs::FS aFS) {
    File file = aFS.open(filename.c_str(), FILE_READ);
    if(!file || !file.size()){
        last_error = 3;
        Serial.printf("[%s] Can't load settings. Setup is default\n", TAG);
    } else {
        Serial.printf("[%s] Load settings\n", TAG);
        size_t size = file.size();
        JsonDocument json;
        deserializeJson(json, file);
        settings.accept_call = json["accept_call"].as<bool>();
        settings.delivery = json["delivery"].as<bool>();
        settings.reject_call = json["reject_call"].as<bool>();
        settings.led = json["led"].as<bool>();
        settings.sound = json["sound"].as<bool>();
        settings.mute = json["mute"].as<bool>();
        settings.greeting = json["greeting"].as<bool>();
        settings.phone_disable = json["phone_disable"].as<bool>();
        settings.modes = json["modes"].as<uint8_t>();
        settings.server_type = json["server_type"].as<uint8_t>();
        settings.delay_before = json["delay_before"].as<uint16_t>();
        settings.delay_open = json["delay_open"].as<uint16_t>();
        settings.delay_after = json["delay_after"].as<uint16_t>();
        settings.delay_filter = json["delay_filter"].as<uint16_t>();
        settings.call_end_delay = json["call_end_delay"].as<uint16_t>();
        settings.greeting_delay = json["greeting_delay"].as<uint16_t>();
        settings.mqtt_port = json["mqtt_port"].as<uint16_t>();
        settings.wifi_ssid = json["ssid"].as<std::string>();
        settings.wifi_passwd = json["wifi_passwd"].as<std::string>();
        settings.mqtt_server = json["mqtt_server"].as<std::string>();
        settings.mqtt_login = json["mqtt_login"].as<std::string>();
        settings.mqtt_passwd = json["mqtt_passwd"].as<std::string>();
        settings.tlg_token = json["tlg_token"].as<std::string>();
        settings.tlg_user = json["tlg_user"].as<std::string>();
        settings.access_code = json["access_code"].as<std::string>();
        settings.user_login = json["user_login"].as<std::string>();
        settings.user_passwd = json["user_passwd"].as<std::string>();
        settings.web_auth = json["web_auth"].as<bool>();
        settings.mqtt_retain = json["mqtt_retain"].as<bool>();
        settings.child_lock = json["child_lock"].as<bool>();
        settings.access_code_lifetime = json["access_code_lifetime"].as<uint16_t>();
        settings.address_counter = json["address_counter"].as<uint8_t>();
        CheckDefault();
        last_error = 0;
    }
    file.close();
}


void SettingsManager::SaveSettings(fs::FS aFS) {
  File file = aFS.open(filename.c_str(), FILE_WRITE);
  if(!file){
    last_error = 4;
    Serial.printf("[%s] Can't save settings\n", TAG);
  } else {
        JsonDocument json;
        json["accept_call"] = settings.accept_call;
        json["delivery"] = settings.delivery;
        json["reject_call"] = settings.reject_call;
        json["led"] = settings.led;
        json["sound"] = settings.sound;
        json["greeting"] = settings.greeting;
        json["mute"] = settings.mute;
        json["phone_disable"] = settings.phone_disable;
        json["modes"] = settings.modes;
        json["server_type"] = settings.server_type;
        json["delay_before"] = settings.delay_before;
        json["delay_open"] = settings.delay_open;
        json["delay_after"] = settings.delay_after;
        json["delay_filter"] = settings.delay_filter;
        json["call_end_delay"] = settings.call_end_delay;
        json["greeting_delay"] = settings.greeting_delay;
        json["mqtt_port"] = settings.mqtt_port;
        json["ssid"] = settings.wifi_ssid;
        json["wifi_passwd"] = settings.wifi_passwd;
        json["mqtt_server"] = settings.mqtt_server;
        json["mqtt_login"] = settings.mqtt_login;
        json["mqtt_passwd"] = settings.mqtt_passwd;
        json["tlg_token"] = settings.tlg_token;
        json["tlg_user"] = settings.tlg_user;
        json["access_code"] = settings.access_code;
        json["user_login"] = settings.user_login;
        json["user_passwd"] = settings.user_passwd;
        json["web_auth"] = settings.web_auth;
        json["mqtt_retain"] = settings.mqtt_retain;
        json["child_lock"] = settings.child_lock;
        json["access_code_lifetime"] = settings.access_code_lifetime;
        json["address_counter"] = settings.address_counter;
        serializeJson(json, file);
        last_error = 0;
  }
  file.close();
  Serial.printf("[%s] Settings saved!\n", TAG);
}

void SettingsManager::ResetSettings() {
    Serial.printf("[%s] Settings reset", TAG);
    settings.accept_call = false;
    settings.delivery = false;
    settings.reject_call = false;
    settings.led = true;
    settings.sound = true;
    settings.mute = false;
    settings.greeting = false;
    settings.phone_disable = false;
    settings.modes = 0;
    settings.server_type = 0;
    settings.delay_before = 1000;
    settings.greeting_delay = 1000;
    settings.delay_open = 600;
    settings.delay_after = 3000;
    settings.delay_filter = 10;
    settings.call_end_delay = 6000;
    settings.mqtt_port = 1883;
    settings.wifi_ssid = "";
    settings.wifi_passwd = "";
    settings.mqtt_server = "";
    settings.mqtt_login = "";
    settings.mqtt_passwd = "";
    settings.tlg_token = "";
    settings.tlg_user = "";
    settings.access_code = "";
    settings.user_login = "";
    settings.user_passwd = "";
    settings.web_auth = false;
    settings.child_lock = false;
    settings.mqtt_retain = true;
    settings.access_code_lifetime = 0;
    settings.address_counter = 0;
    last_error = 0;
}

std::string SettingsManager::getSettings(){
    JsonDocument json;
    json["accept_call"] = settings.accept_call;
    json["delivery"] = settings.delivery;
    json["reject_call"] = settings.reject_call;
    json["led"] = settings.led;
    json["sound"] = settings.sound;
    json["greeting"] = settings.greeting;
    json["mute"] = settings.mute;
    json["phone_disable"] = settings.phone_disable;
    json["modes"] = settings.modes;
    json["server_type"] = settings.server_type;
    json["delay_before"] = settings.delay_before;
    json["delay_open"] = settings.delay_open;
    json["delay_after"] = settings.delay_after;
    json["delay_filter"] = settings.delay_filter;
    json["call_end_delay"] = settings.call_end_delay;
    json["greeting_delay"] = settings.greeting_delay;
    json["mqtt_port"] = settings.mqtt_port;
    json["ssid"] = settings.wifi_ssid;
    json["wifi_passwd"] = settings.wifi_passwd;
    json["mqtt_server"] = settings.mqtt_server;
    json["mqtt_login"] = settings.mqtt_login;
    json["mqtt_passwd"] = settings.mqtt_passwd;
    json["tlg_token"] = settings.tlg_token;
    json["tlg_user"] = settings.tlg_user;
    json["access_code"] = settings.access_code==""?"------":settings.access_code;
    json["access_code_lifetime"] = settings.access_code_lifetime;
    json["user_login"] = settings.user_login;
    json["user_passwd"] = settings.user_passwd;
    json["web_auth"] = settings.web_auth;
    json["child_lock"] = settings.child_lock;
    json["mqtt_retain"] = settings.mqtt_retain;
    json["address_counter"] = settings.address_counter;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setMode(uint8_t value) {
    settings.modes = value;
    JsonDocument json;
    json["modes"] = settings.modes;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setAccept(bool value) {
    settings.accept_call = value;
    if (value) settings.reject_call = false;
    else settings.delivery = false;
    JsonDocument json;
    json["accept_call"] = settings.accept_call;
    json["delivery"] = settings.delivery;
    json["reject_call"] = settings.reject_call;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setDelivery(bool value) {
    settings.delivery = value;
    if (value) {
        settings.accept_call = true;
        settings.reject_call = false;
    }
    JsonDocument json;
    json["accept_call"] = settings.accept_call;
    json["delivery"] = settings.delivery;
    json["reject_call"] = settings.reject_call;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setReject(bool value) {
    settings.reject_call = value;
    if (value) {
        settings.accept_call = false;
        settings.delivery = false;
    }
    JsonDocument json;
    json["accept_call"] = settings.accept_call;
    json["delivery"] = settings.delivery;
    json["reject_call"] = settings.reject_call;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setDelayBeforeAnswer(uint16_t value) {
    settings.delay_before = value;
    JsonDocument json;
    json["delay_before"] = settings.delay_before;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setDelayOpen(uint16_t value) {
    settings.delay_open = value;
    JsonDocument json;
    json["delay_open"] = settings.delay_open;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setDelayAfterClose(uint16_t value) {
    settings.delay_after = value;
    JsonDocument json;
    json["delay_after"] = settings.delay_after;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setCodeLifeTime(uint16_t value) {
    settings.access_code_lifetime = value;
    JsonDocument json;
    json["access_code_lifetime"] = settings.access_code_lifetime;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setDelayFilter(uint16_t value) {
    settings.delay_filter = value;
    JsonDocument json;
    json["delay_filter"] = settings.delay_filter;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setCallEndDelay(uint16_t value) {
    settings.call_end_delay = value;
    JsonDocument json;
    json["call_end_delay"] = settings.call_end_delay;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setAddressCounter(uint8_t value) {
    settings.address_counter = value;
    JsonDocument json;
    json["address_counter"] = settings.address_counter;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setGreetingDelay(uint16_t value) {
    settings.greeting_delay = value;
    JsonDocument json;
    json["greeting_delay"] = settings.greeting_delay;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setLed(bool value) {
    settings.led = value;
    JsonDocument json;
    json["led"] = settings.led;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setChildLock(bool value) {
    settings.child_lock = value;
    JsonDocument json;
    json["child_lock"] = settings.child_lock;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setRetain(bool value) {
    settings.mqtt_retain = value;
    JsonDocument json;
    json["mqtt_retain"] = settings.mqtt_retain;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setSound(bool value) {
    settings.sound = value;
    JsonDocument json;
    json["sound"] = settings.sound;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setMute(bool value) {
    settings.mute = value;
    JsonDocument json;
    json["mute"] = settings.mute;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setPhoneDisable(bool value) {
    settings.phone_disable = value;
    JsonDocument json;
    json["phone_disable"] = settings.phone_disable;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setSSID(std::string value) {
    settings.wifi_ssid = value;
    JsonDocument json;
    json["ssid"] = settings.wifi_ssid;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setWIFIPassword(std::string value) {
    settings.wifi_passwd = value;
    JsonDocument json;
    json["wifi_passwd"] = settings.wifi_passwd;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setServerType(uint8_t value) {
    settings.server_type = value;
    JsonDocument json;
    json["server_type"] = settings.server_type;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setMQTTServer(std::string value) {
    settings.mqtt_server = value;
    JsonDocument json;
    json["mqtt_server"] = settings.mqtt_server;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setMQTTPort(uint16_t value) {
    settings.mqtt_port = value;
    JsonDocument json;
    json["mqtt_port"] = settings.mqtt_port;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setMQTTLogin(std::string value) {
    settings.mqtt_login = value;
    JsonDocument json;
    json["mqtt_login"] = settings.mqtt_login;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setMQTTPassword(std::string value) {
    settings.mqtt_passwd = value;
    JsonDocument json;
    json["mqtt_passwd"] = settings.mqtt_passwd;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setTLGToken(std::string value) {
    settings.tlg_token = value;
    JsonDocument json;
    json["tlg_token"] = settings.tlg_token;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setTLGUser(std::string value) {
    settings.tlg_user = value;
    JsonDocument json;
    json["tlg_user"] = settings.tlg_user;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setUserLogin(std::string value) {
    settings.user_login = value;
    JsonDocument json;
    json["user_login"] = settings.user_login;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setUserPassword(std::string value) {
    settings.user_passwd = value;
    JsonDocument json;
    json["user_passwd"] = settings.user_passwd;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setTLGCode(std::string value) {
    settings.access_code = value;
    JsonDocument json;
    json["access_code"] = settings.access_code==""?"------":settings.access_code;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setAuth(bool value) {
    settings.web_auth = value;
    JsonDocument json;
    json["web_auth"] = settings.web_auth;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setGreeting(bool value) {
    settings.greeting = value;
    JsonDocument json;
    json["greeting"] = settings.greeting;
    serializeJson(json, message);
    Serial.printf("[%s] %s\n", TAG, message.c_str());
    return message;
}