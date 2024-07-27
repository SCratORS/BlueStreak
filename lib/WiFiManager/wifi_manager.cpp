#include "wifi_manager.h"
#include "esp_log.h"

static const char* TAG = "WIFI";

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
    ESP_LOGI(TAG, "Disconnect from SSID: %s", ssid);
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
            ESP_LOGI(TAG, "Connecting to SSID: %s", ssid.c_str());
            WiFi.begin(ssid.c_str(), passwd.c_str());
            timer = millis();
            status.connecting_wifi = true;
        } else { //Подключение запущено
            if (millis() - timer > 15000) { // Ждем 15 секунд
                ESP_LOGI(TAG, "Connecting to SSID failure", ssid.c_str());
                if (!status.start_ap) {
                    //Поднимаем свою AP
                    ip = WiFi.softAPIP().toString().c_str();
                    ESP_LOGI(TAG, "Activate software Wi-Fi AccessPoint: %s", CONFIG_CHIP_DEVICE_PRODUCT_NAME);
                    ESP_LOGI(TAG, "Use IP address for access to web ui: http://%s", ip.c_str());
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
                ESP_LOGI(TAG, "Disable software Wi-Fi AccessPoint");
                WiFi.mode(WIFI_STA);
                dnsServer.stop();
                status.start_ap = false;
            }
        }
        ip = WiFi.localIP().toString().c_str();
        last_error = 0;
        if (status.connecting_wifi) {
            ESP_LOGI(TAG, "Conneting to Wi-Fi network successful. IP address: %s", ip.c_str());
            status.connecting_wifi = false;
        }
    }
    if (status.start_ap) dnsServer.processNextRequest();
}