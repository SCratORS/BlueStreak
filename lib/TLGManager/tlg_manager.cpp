#include "tlg_manager.h"
#include "esp_log.h"

static const char * TAG = "TLG";

TLGManager::TLGManager(std::string token){
    tlg_client = new FastBot(token.c_str());
    tlg_started = false;
}

TLGManager::~TLGManager() {
    tlg_started = false;
    delete (tlg_client); tlg_client = nullptr;
}

void TLGManager::sendMenu(std::string wellcome, std::string message, std::string command, bool edit, std::string chat_id){
    if (chat_id == "") return;
    setUser(chat_id);
    if (edit && menu_id!=0) {
        tlg_client->editMenuCallback(menu_id, message.c_str(), command.c_str());
    } else {
        tlg_client->inlineMenuCallback(wellcome.c_str(), message.c_str(), command.c_str());
        menu_id = tlg_client->lastBotMsg();
    }
    setUser("");
}

void TLGManager::sendMessage(std::string message, std::string chat_id) {
    if (chat_id == "") return;
    tlg_client->sendMessage(message.c_str(), chat_id.c_str());
}

void TLGManager::setUser(std::string chat_id) {
    if (chat_id == "") tlg_client->setChatID(0);
    else tlg_client->setChatID(chat_id.c_str());
}

void TLGManager::setToken(std::string token) {
    tlg_client->setToken(token.c_str());
}

void TLGManager::handle() {
    if (!tlg_started) return;
    if (tlg_client->tick()>1) last_error = 5; 
    else last_error = 0;
}