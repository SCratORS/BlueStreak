#include <FastBot.h>

class TLGManager {
    public:
        TLGManager(std::string token);
        ~TLGManager();
        void handle();
        void setToken(std::string token);
        void setUser(std::string chat_id);
        FastBot * getTLGClient() {return tlg_client;}
        void begin() {tlg_started = true;}
        void sendMenu(std::string, std::string, std::string, bool, std::string);
        void sendMessage(std::string, std::string);
        uint8_t last_error = 0;

    private:
        bool tlg_started = false;
        FastBot * tlg_client;
        int32_t menu_id = 0;
        void messagesHandle(FB_msg& msg);
        
};