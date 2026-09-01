#include "stdint.h"
#include <string>
#include <DNSServer.h>
#include "WiFi.h"
#include <WiFiUdp.h>

#define CONFIG_CHIP_DEVICE_PRODUCT_NAME "SmartIntercom"

class WiFiManager {
    public:
        WiFiManager(const std::string &ssid, const std::string &passwd, const std::string &bssid);
        ~WiFiManager();
        void handle();
        void setPasswd(const std::string &passwd);
        void setSSID(const std::string &ssid);
        void setBSSID(const std::string &value);
        void disconnect();
        std::string ip;
        uint8_t last_error;
        bool Connected() {return status.connect_wifi && !status.connecting_wifi;}
    private:
        struct {
            bool connect_wifi = false;
            bool connecting_wifi = false;
            bool start_ap = false;
         } status;
        DNSServer dnsServer;
        uint64_t timer;
        std::string ssid;
        std::string passwd;
        uint8_t bssid[6];
        bool use_bssid = false;
};