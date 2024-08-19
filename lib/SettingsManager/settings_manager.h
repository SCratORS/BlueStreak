#include "stdint.h"
#include <string>
#include <FS.h>

#define DEFAULT_TIME_SERVER "pool.ntp.org"

class SettingsManager {
    public:
        SettingsManager(std::string filename);
        ~SettingsManager();
        void CheckDefault();
        void LoadSettings(fs::FS aFS);
        void SaveSettings(fs::FS aFS);
        void ResetSettings();
        uint8_t last_error = 0;
        uint64_t access_code_expires = 0;
        struct {
            bool accept_call = false;
            bool delivery = false;
            bool reject_call = false;
            bool led = true;
            bool sound = true;
            bool greeting = false;
            bool mute = false;
            bool phone_disable = false;
            bool ftp = false;
            uint8_t modes = 0;
            uint8_t server_type = 0;
            uint16_t delay_before = 1000;
            uint16_t delay_open = 600;
            uint16_t delay_after = 3000;
            uint16_t delay_filter  = 10;
            uint16_t call_end_delay  = 6000;
            u_int16_t greeting_delay = 1000;
            const uint16_t delay_before_open_door = 100;
            std::string time_server = DEFAULT_TIME_SERVER;
            std::string wifi_ssid = "";
            std::string wifi_passwd = "";
            std::string mqtt_server = "";
            uint16_t mqtt_port = 1883;
            std::string mqtt_login = "";
            std::string mqtt_passwd = "";
            std::string tlg_token = "";
            std::string tlg_user = "";
            std::string access_code = "";
            std::string user_login = "";
            std::string user_passwd = "";
            bool web_auth = false;
            bool child_lock = false;
            bool mqtt_retain = true;
            uint16_t access_code_lifetime = 0;
        } settings;
        std::string getSettings();
        std::string setMode(uint8_t value);
        std::string setAccept(bool value);
        std::string setDelivery(bool value);
        std::string setReject(bool value);
        std::string setDelayBeforeAnswer(uint16_t value);
        std::string setDelayOpen(uint16_t value);
        std::string setDelayAfterClose(uint16_t value);
        std::string setCallEndDelay(uint16_t value);
        std::string setDelayFilter(uint16_t value);
        std::string setCodeLifeTime(uint16_t value);
        std::string setGreetingDelay(uint16_t value);
        std::string setLed(bool value);
        std::string setSound(bool value);
        std::string setMute(bool value);
        std::string setRetain(bool value);
        std::string setPhoneDisable(bool value);
        std::string setSSID(std::string value);
        std::string setWIFIPassword(std::string value);
        std::string setServerType(uint8_t value);
        std::string setMQTTServer(std::string value);
        std::string setMQTTPort(uint16_t value);
        std::string setMQTTLogin(std::string value);
        std::string setMQTTPassword(std::string value);
        std::string setTLGToken(std::string value);
        std::string setTLGUser(std::string value);
        std::string setTLGCode(std::string value); 
        std::string setAuth(bool value);
        std::string setGreeting(bool value);
        std::string setChildLock(bool value);
        std::string setUserLogin(std::string value);
        std::string setUserPassword(std::string value);

    private:
        std::string filename;
};