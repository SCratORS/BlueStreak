#include "stdint.h"
#include <string>
#include <DNSServer.h>
#include "WiFi.h"

#define CONFIG_CHIP_DEVICE_PRODUCT_NAME "SmartIntercom"

class WiFiManager {
    public:
        WiFiManager(std::string ssid, std::string passwd);
        ~WiFiManager();
        void handle();
        void setPasswd(std::string passwd);
        void setSSID(std::string ssid);
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
};