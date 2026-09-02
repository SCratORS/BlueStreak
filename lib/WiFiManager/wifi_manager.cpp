#include "wifi_manager.h"

static const char* TAG = "WIFI";
extern void LOG(const char * format, ...);

static uint8_t hex(uint8_t value) {
    if (value >= '0' && value <= '9') return value - '0';
    value |= 0x20;
    return value >= 'a' && value <= 'f' ? value - 'a' + 10 : 16;
}

WiFiManager::WiFiManager(const std::string &ssid, const std::string &passwd, const std::string &bssid)
    : ssid(ssid), passwd(passwd) {
    setBSSID(bssid);
    last_error = 1;
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(CONFIG_CHIP_DEVICE_PRODUCT_NAME);
    handle();
}

void WiFiManager::setPasswd(const std::string &passwd) {
    this->passwd = passwd;
};

void WiFiManager::setSSID(const std::string &ssid) {
    this->ssid = ssid;
};

void WiFiManager::setBSSID(const std::string &value) {
    use_bssid = false;
    if (value.length() != 17) return;
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t offset = i * 3;
        uint8_t high = hex(value[offset]);
        uint8_t low = hex(value[offset + 1]);
        if (high > 15 || low > 15 || (i < 5 && value[offset + 2] != ':')) return;
        bssid[i] = high << 4 | low;
    }
    use_bssid = true;
};

void WiFiManager::disconnect() {
    WiFi.disconnect();
    LOG("[%s] Disconnect from SSID: %s\n", TAG, ssid);
};

void WiFiManager::handle()   {
    status.connect_wifi = (WiFi.status() == WL_CONNECTED);
    if (!status.connect_wifi) {   //Не подключены к wifi
        if (!status.connecting_wifi) { //Не пытались подключаться к wifi
            if (!status.start_ap) {
                WiFi.mode(WIFI_STA);
                last_error = 1;
            }
            WiFi.disconnect();
            LOG("[%s] Connecting to SSID: %s\n", TAG, ssid.c_str());
            WiFi.begin(ssid.c_str(), passwd.c_str(), 0, use_bssid ? bssid : nullptr);
            timer = millis();
            status.connecting_wifi = true;
        } else { //Подключение запущено
            if (millis() - timer > 15000) { // Ждем 15 секунд
                LOG("[%s] Connecting to SSID failure\n", TAG);
                if (!status.start_ap) {
                    //Поднимаем свою AP
                    ip = WiFi.softAPIP().toString().c_str();
                    LOG("[%s] Activate software Wi-Fi AccessPoint: %s\n", TAG, CONFIG_CHIP_DEVICE_PRODUCT_NAME);
                    LOG("[%s] Use IP address for access to web ui: http://%s\n", TAG, ip.c_str());
                    WiFi.mode(WIFI_AP_STA);
                    WiFi.softAP(CONFIG_CHIP_DEVICE_PRODUCT_NAME, NULL);
                    dnsServer.start(53, "*", WiFi.softAPIP());
                    last_error = 2;
                    status.start_ap = true;
                }
                status.connecting_wifi = false;
            }
        }
    } else {
        if (status.start_ap) {
            if (!WiFi.softAPgetStationNum()) {
                LOG("[%s] Disable software Wi-Fi AccessPoint\n", TAG);
                WiFi.mode(WIFI_STA);
                dnsServer.stop();
                status.start_ap = false;
            }
        }
        ip = WiFi.localIP().toString().c_str();
        last_error = 0;
        if (status.connecting_wifi) {
            LOG("[%s] Conneting to Wi-Fi network successful. IP address: %s\n", TAG, ip.c_str());
            status.connecting_wifi = false;
        }
    }
    if (status.start_ap) dnsServer.processNextRequest();
}