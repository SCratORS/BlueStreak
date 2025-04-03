#include "wifi_manager.h"

static const char* TAG = "WIFI";
extern void LOG(const char * format, ...);

WiFiManager::WiFiManager(std::string ssid, std::string passwd) {
    this->ssid = ssid;
    this->passwd = passwd;
    last_error = 1;
    handle();
}

void WiFiManager::setPasswd(std::string passwd) {
    this->passwd = passwd;
};

void WiFiManager::setSSID(std::string ssid) {
    this->ssid = ssid;
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
            WiFi.begin(ssid.c_str(), passwd.c_str());
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