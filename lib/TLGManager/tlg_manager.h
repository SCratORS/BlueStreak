#include "stdint.h"
#include <string>
#include <HTTPClient.h>
#include "settings_manager.h"

class TLGManager {
    public:
        TLGManager();
        ~TLGManager();
        bool begin();
        void setToken(std::string token) { this->host = "https://api.telegram.org/bot" + token + "/"; }
        void setUpdate(uint64_t update_id) {this->update_id = update_id;}
        void sendMenu();
        bool sendMessage(std::string, std::string, bool force = false, std::string mode = "");
        void (*message)(std::string, std::string, std::string);
        void (*callback_query)(std::string, std::string, std::string);
        bool sendControlPanel(bool, std::string);
        bool sendSettingsPanel(bool, std::string);
        bool sendModeKeyboard(bool, std::string);
        bool sendStartMessage(std::string);
        void getUpdate();
        bool enabled() {return tlg_started;}
        void stop(); 
        std::string post(std::string, bool force = false);
        uint8_t last_error = 0;
        uint64_t menu_id = 0;
        SettingsManager * settings_manager;
    private:
        HTTPClient * https_client;
        HTTPClient * https_client_post;
        std::string host;
        bool tlg_started = false;
        bool update_loop = false;
        uint64_t update_id = 0;
        bool getMe();
        bool setMyCommands();
        bool answerCallbackQuery(std::string);
};