#include "settings_manager.h"


static const char* TAG = "SETTING";
extern void LOG(const char * format, ...);

SettingsManager::SettingsManager(std::string filename) {
    this->filename = filename;
    last_error= 0;
}

SettingsManager::~SettingsManager() {
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
    if (settings.syslog_server == "null") settings.syslog_server = "";
    if (settings.dev_name == "null") settings.dev_name = "smartintercom";
    if (settings.syslog_port == 0) settings.syslog_port = 514;
    if (settings.delay_before == 0) settings.delay_before = 1000;
    if (settings.delay_before == 0) settings.delay_before = 1000;
    if (settings.delay_open == 0) settings.delay_open = 600;
    if (settings.delay_after == 0) settings.delay_after = 3000;
    if (settings.delay_filter == 0) settings.delay_filter = 10;
    if (settings.call_end_delay == 0) settings.call_end_delay = 6000;    
    if (settings.greeting_delay == 0) settings.greeting_delay = 1000;
    if (settings.delay_system == 0) settings.delay_system = 500;
    if (settings.mqtt_port == 0) settings.mqtt_port = 1883;
    if (settings.counter_duration == 0) settings.counter_duration = 190;
    if (settings.impulse_filter == 0) settings.impulse_filter = 100;
}

void SettingsManager::LoadSettings(fs::FS aFS) {
    File file = aFS.open(filename.c_str(), FILE_READ);
    if(!file || !file.size()){
        last_error = 3;
        LOG("[%s] Can't load settings. Setup is default\n", TAG);
    } else {
        LOG("[%s] Load settings\n", TAG);
        size_t size = file.size();
        json.clear();
        deserializeJson(json, file);
        settings.accept_call = json["accept_call"].as<bool>();
        settings.delivery = json["delivery"].as<bool>();
        settings.reject_call = json["reject_call"].as<bool>();
        settings.led = json["led"].as<bool>();
        settings.sound = json["sound"].as<bool>();
        settings.mute = json["mute"].as<bool>();
        settings.greeting = json["greeting"].as<bool>();
        settings.ringtone = json["ringtone"].as<bool>();
        settings.phone_disable = json["phone_disable"].as<bool>();
        settings.modes = json["modes"].as<uint8_t>();
        settings.server_type = json["server_type"].as<uint8_t>();
        settings.delay_system = json["delay_system"].as<uint16_t>();
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
        settings.syslog = json["syslog"].as<bool>();
        settings.syslog_port = json["syslog_port"].as<uint16_t>();
        settings.syslog_server = json["syslog_server"].as<std::string>();
        settings.force_open = json["force_open"].as<bool>();
        settings.dev_name = json["dev_name"].as<std::string>();
        settings.reboot_timeout = json["reboot_timeout"].as<uint8_t>();
        settings.counter_duration = json["counter_duration"].as<uint16_t>();
        settings.impulse_filter = json["impulse_filter"].as<uint16_t>();
        CheckDefault();
        last_error = 0;
    }
    file.close();
}


void SettingsManager::SaveSettings(fs::FS aFS) {
  File file = aFS.open(filename.c_str(), FILE_WRITE);
  if(!file){
    last_error = 4;
    LOG("[%s] Can't save settings\n", TAG);
  } else {
        json.clear();
        json["accept_call"] = settings.accept_call;
        json["delivery"] = settings.delivery;
        json["reject_call"] = settings.reject_call;
        json["led"] = settings.led;
        json["sound"] = settings.sound;
        json["greeting"] = settings.greeting;
        json["ringtone"] = settings.ringtone;
        json["mute"] = settings.mute;
        json["phone_disable"] = settings.phone_disable;
        json["modes"] = settings.modes;
        json["server_type"] = settings.server_type;
        json["delay_system"] = settings.delay_system;
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
        json["syslog"] = settings.syslog;
        json["syslog_port"] = settings.syslog_port;
        json["syslog_server"] = settings.syslog_server;
        json["force_open"] = settings.force_open;
        json["dev_name"] = settings.dev_name;
        json["reboot_timeout"] = settings.reboot_timeout;
        json["counter_duration"] = settings.counter_duration;
        json["impulse_filter"] = settings.impulse_filter;
        serializeJson(json, file);
        last_error = 0;
  }
  file.close();
  LOG("[%s] Settings saved!\n", TAG);
}

void SettingsManager::ResetSettings() {
    LOG("[%s] Settings reset", TAG);
    settings.accept_call = false;
    settings.delivery = false;
    settings.reject_call = false;
    settings.led = true;
    settings.sound = true;
    settings.mute = false;
    settings.greeting = false;
    settings.ringtone = false;
    settings.phone_disable = false;
    settings.modes = 0;
    settings.server_type = 0;
    settings.delay_system = 500;
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
    settings.syslog = false;
    settings.syslog_port = 514;
    settings.syslog_server = "";
    settings.force_open = false;
    settings.dev_name = "smartintercom";
    settings.reboot_timeout = 0;
    settings.counter_duration = 190;
    settings.impulse_filter = 100;
    last_error = 0;
}

std::string SettingsManager::getSettings(){
    json.clear();
    json["accept_call"] = settings.accept_call;
    json["delivery"] = settings.delivery;
    json["reject_call"] = settings.reject_call;
    json["led"] = settings.led;
    json["sound"] = settings.sound;
    json["greeting"] = settings.greeting;
    json["ringtone"] = settings.ringtone;
    json["mute"] = settings.mute;
    json["phone_disable"] = settings.phone_disable;
    json["modes"] = settings.modes;
    json["server_type"] = settings.server_type;
    json["delay_system"] = settings.delay_system;
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
    json["syslog"] = settings.syslog;
    json["syslog_port"] = settings.syslog_port;
    json["syslog_server"] = settings.syslog_server;
    json["force_open"] = settings.force_open;
    json["dev_name"] = settings.dev_name;
    json["reboot_timeout"] = settings.reboot_timeout;
    json["counter_duration"] = settings.counter_duration;
    json["impulse_filter"] = settings.impulse_filter;
    serializeJson(json, message);
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setMode(uint8_t value) {
    settings.modes = value;
    message = "{\"modes\":" + std::to_string(settings.modes) + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setAccept(bool value) {
    settings.accept_call = value;
    if (value) settings.reject_call = false;
    else settings.delivery = false;
    message = "{\"accept_call\":" + std::string(settings.accept_call?"true":"false") + "," +
                "\"delivery\":" + std::string(settings.delivery?"true":"false") + "," +
                "\"reject_call\":" + std::string(settings.reject_call?"true":"false") + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setDelivery(bool value) {
    settings.delivery = value;
    if (value) {
        settings.accept_call = true;
        settings.reject_call = false;
    }
    message = "{\"accept_call\":" + std::string(settings.accept_call?"true":"false") + "," +
                "\"delivery\":" + std::string(settings.delivery?"true":"false") + "," +
                "\"reject_call\":" + std::string(settings.reject_call?"true":"false") + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setReject(bool value) {
    settings.reject_call = value;
    if (value) {
        settings.accept_call = false;
        settings.delivery = false;
    }
    message = "{\"accept_call\":" + std::string(settings.accept_call?"true":"false") + "," +
                "\"delivery\":" + std::string(settings.delivery?"true":"false") + "," +
                "\"reject_call\":" + std::string(settings.reject_call?"true":"false") + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setDelaySystem(uint16_t value) {
    settings.delay_system = value;
    message = "{\"delay_system\":" + std::to_string(settings.delay_system) + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setDelayBeforeAnswer(uint16_t value) {
    settings.delay_before = value;
    message = "{\"delay_before\":" + std::to_string(settings.delay_before) + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setDelayOpen(uint16_t value) {
    settings.delay_open = value;
    message = "{\"delay_open\":" + std::to_string(settings.delay_open) + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setDelayAfterClose(uint16_t value) {
    settings.delay_after = value;
    message = "{\"delay_after\":" + std::to_string(settings.delay_after) + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setCodeLifeTime(uint16_t value) {
    settings.access_code_lifetime = value;
    message = "{\"access_code_lifetime\":" + std::to_string(settings.access_code_lifetime) + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setDelayFilter(uint16_t value) {
    settings.delay_filter = value;
    message = "{\"delay_filter\":" + std::to_string(settings.delay_filter) + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setCallEndDelay(uint16_t value) {
    settings.call_end_delay = value;
    message = "{\"call_end_delay\":" + std::to_string(settings.call_end_delay) + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setAddressCounter(uint8_t value) {
    settings.address_counter = value;
    message = "{\"address_counter\":" + std::to_string(settings.address_counter) + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setGreetingDelay(uint16_t value) {
    settings.greeting_delay = value;
    message = "{\"greeting_delay\":" + std::to_string(settings.greeting_delay) + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setLed(bool value) {
    settings.led = value;
    message = "{\"led\":" + std::string(settings.led?"true":"false") + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setChildLock(bool value) {
    settings.child_lock = value;
    message = "{\"child_lock\":" + std::string(settings.child_lock?"true":"false") + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setRetain(bool value) {
    settings.mqtt_retain = value;
    message = "{\"mqtt_retain\":" + std::string(settings.mqtt_retain?"true":"false") + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setSound(bool value) {
    settings.sound = value;
    message = "{\"sound\":" + std::string(settings.sound?"true":"false") + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setMute(bool value) {
    settings.mute = value;
    message = "{\"mute\":" + std::string(settings.mute?"true":"false") + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setPhoneDisable(bool value) {
    settings.phone_disable = value;
    message = "{\"phone_disable\":" + std::string(settings.phone_disable?"true":"false") + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setSSID(std::string value) {
    settings.wifi_ssid = value;
    message = "{\"ssid\":\"" + settings.wifi_ssid + "\"}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setWIFIPassword(std::string value) {
    settings.wifi_passwd = value;
    message = "{\"wifi_passwd\":\"" + settings.wifi_passwd + "\"}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setServerType(uint8_t value) {
    settings.server_type = value;
    message = "{\"server_type\":" + std::to_string(settings.server_type) + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setMQTTServer(std::string value) {
    settings.mqtt_server = value;
    message = "{\"mqtt_server\":\"" + settings.mqtt_server + "\"}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setMQTTPort(uint16_t value) {
    settings.mqtt_port = value;
    message = "{\"mqtt_port\":" + std::to_string(settings.mqtt_port) + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setMQTTLogin(std::string value) {
    settings.mqtt_login = value;
    message = "{\"mqtt_login\":\"" + settings.mqtt_login + "\"}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setMQTTPassword(std::string value) {
    settings.mqtt_passwd = value;
    message = "{\"mqtt_passwd\":\"" + settings.mqtt_passwd + "\"}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setDevName(std::string value) {
    settings.dev_name = value;
    message = "{\"dev_name\":\"" + settings.dev_name + "\"}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setTLGToken(std::string value) {
    settings.tlg_token = value;
    message = "{\"tlg_token\":\"" + settings.tlg_token + "\"}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setTLGUser(std::string value) {
    settings.tlg_user = value;
    message = "{\"tlg_user\":\"" + settings.tlg_user + "\"}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setUserLogin(std::string value) {
    settings.user_login = value;
    message = "{\"user_login\":\"" + settings.user_login + "\"}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setUserPassword(std::string value) {
    settings.user_passwd = value;
    message = "{\"user_passwd\":\"" + settings.user_passwd + "\"}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setTLGCode(std::string value) {
    settings.access_code = value;
    message = "{\"access_code\":\"" + std::string(settings.access_code==""?"------":settings.access_code) + "\"}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setAuth(bool value) {
    settings.web_auth = value;
    message = "{\"web_auth\":" + std::string(settings.web_auth?"true":"false") + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setGreeting(bool value) {
    settings.greeting = value;
    message = "{\"greeting\":" + std::string(settings.greeting?"true":"false") + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setRingtone(bool value) {
    settings.ringtone = value;
    message = "{\"ringtone\":" + std::string(settings.ringtone?"true":"false") + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setForceOpen(bool value) {
    settings.force_open = value;
    message = "{\"force_open\":" + std::string(settings.force_open?"true":"false") + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}
std::string SettingsManager::setSysLog(bool value) {
    settings.syslog = value;
    message = "{\"syslog\":" + std::string(settings.syslog?"true":"false") + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}
std::string SettingsManager::setSysLogPort(uint16_t value) {
    settings.syslog_port = value;
    message = "{\"syslog_port\":" + std::to_string(settings.syslog_port) + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}
std::string SettingsManager::setSysLogServer(std::string value) {
    settings.syslog_server = value;
    message = "{\"syslog_server\":\"" + settings.syslog_server + "\"}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setRebootTimeout(uint8_t value) {
    settings.reboot_timeout = value;
    message = "{\"reboot_timeout\":" + std::to_string(settings.reboot_timeout) + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setCounterDuration(uint16_t value) {
    settings.counter_duration = value;
    message = "{\"counter_duration\":" + std::to_string(settings.counter_duration) + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}

std::string SettingsManager::setImpulseFilter(uint16_t value) {
    settings.impulse_filter = value;
    message = "{\"impulse_filter\":" + std::to_string(settings.impulse_filter) + "}";
    LOG("[%s] %s\n", TAG, message.c_str());
    return message;
}



