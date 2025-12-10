#include "tlg_manager.h"
#include "ArduinoJson.h"

static const char * TAG = "TLG";
extern std::string mode_name[3];
extern void LOG(const char * format, ...);
static std::string responce;

TLGManager::TLGManager(HTTPClient * https_client, HTTPClient * https_client_post){
    this->https_client = https_client;
    this->https_client_post = https_client_post;
    tlg_started = false;
    host = "";
    menu_id = 0;
    update_id = 0;
}

TLGManager::~TLGManager() {
}

void TLGManager::stop() {
    update_loop = false;
    https_client->setReuse(false);
    https_client->end();
    https_client_post->setReuse(false);
    https_client_post->end();
    tlg_started = false;
} 

std::string TLGManager::post(std::string content, bool force) {
    responce = "{}";
    if (content == "" || content == "{}") {
        LOG("[%s] Json serialize error. Empty request. System will restart.\n", TAG);
        ESP.restart();
    }
    if (force) {   
        update_loop = false;
        LOG("[%s] request force post: %s\n", TAG, content.c_str());
        https_client_post->POST(content.c_str());
        responce = https_client_post->getString().c_str();
    } else {
        LOG("[%s] request post: %s\n", TAG, content.c_str());
        https_client->POST(content.c_str());
        responce = https_client->getString().c_str();
    }
    LOG("[%s] post responce: %s\n", TAG, responce.c_str());
    if (force) update_loop = tlg_started;
    if (responce == "" || responce == "{}") {
        LOG("[%s] incorrect responce. System will restart.\n", TAG);
        ESP.restart();
    } else last_error = 0;
    return responce;
}

bool TLGManager::getMe() {
    JsonDocument responce;
    deserializeJson(responce, (char*)post("{\"method\":\"getMe\"}").c_str());
    return (responce!="") && (responce["ok"].as<bool>());
}

bool TLGManager::setMyCommands() {
    JsonDocument responce;
    deserializeJson(responce, (char*)post("{\"method\":\"setMyCommands\",\"commands\":[{\"command\":\"control\",\"description\":\"Смена режимов работы\"},{\"command\":\"settings\",\"description\":\"Изменение настроек устройства\"}]}").c_str());
    return (responce!="") && (responce["ok"].as<bool>());   
}

bool TLGManager::sendStartMessage(std::string chat_id) {
    std::string welcome = "SmartIntercom: Телеграм-бот.\nПрошивка: "+std::string(CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION)+"\nПоддержка устройства: https://t.me/smartintercom";
    JsonDocument responce;
    deserializeJson(responce, (char*)post("{\"method\":\"sendMessage\",\"chat_id\":\""+ chat_id +"\",\"text\":\"" + welcome + "\",\"reply_markup\":{\"resize_keyboard\":true,\"keyboard\":[[\"✅ Открой дверь\",\"🚚 Открой курьеру\",\"🚷 Сбрось вызов\"]]}}").c_str());
    return (responce!="") && (responce["ok"].as<bool>());
}

bool TLGManager::deletePublicAccess(std::string chat_id){
    if (public_chat_id == "" && chat_id == "") return true;
    JsonDocument responce;
    deserializeJson(responce, (char*)post("{\"method\":\"sendMessage\",\"chat_id\":\""+std::string(chat_id == ""?public_chat_id:chat_id)+"\",\"text\":\""+std::string(chat_id==""?"⚠️ Срок действия кода завершён.":"⛔️ Доступ запрещён.")+"\",\"reply_markup\":{\"remove_keyboard\":true}}", true).c_str());
    public_chat_id = "";
    return (responce!="") && (responce["ok"].as<bool>());
}

void TLGManager::addPublicChatId(std::string chat_id) {
    if (public_chat_id == "") public_chat_id = chat_id;
    else if (public_chat_id.find(chat_id) == std::string::npos) public_chat_id = public_chat_id + "," + chat_id;
}

bool TLGManager::sendOpenKeyboard(std::string chat_id, std::string message) {
    JsonDocument responce;
    deserializeJson(responce, (char*)post("{\"method\":\"sendMessage\",\"chat_id\":\""+chat_id+"\",\"text\":\""+message+"\",\"reply_markup\":{\"resize_keyboard\":true,\"keyboard\":[[\"✅ Открой дверь\"]]}}").c_str());
    return (responce!="") && (responce["ok"].as<bool>());
}

bool TLGManager::sendModeKeyboard(bool edit, std::string chat_id) {
    JsonDocument responce;
    deserializeJson(responce, (char*)post("{\"method\":\""+std::string(edit?"editMessageText":"sendMessage")+"\","+std::string(edit?std::string("\"message_id\":"+std::to_string(menu_id)+","):"")+"\"chat_id\":\""+chat_id+"\",\"text\":\"Выбор постоянного режима работы:\",\"reply_markup\":{\"inline_keyboard\":[[{\"text\":\"Не активен\",\"callback_data\":\"mode_0\"}],[{\"text\":\"Сброс вызова\",\"callback_data\":\"mode_1\"}],[{\"text\":\"Открывать всегда\",\"callback_data\":\"mode_2\"}]]}}").c_str());
    return (responce!="") && (responce["ok"].as<bool>());
 }


bool TLGManager::sendSettingsPanel(bool edit, std::string chat_id){
    JsonDocument responce;
    deserializeJson(responce, (char*)post("{\"method\":\""+std::string(edit?"editMessageText":"sendMessage")+
    "\","+std::string(edit?std::string("\"message_id\":"+std::to_string(menu_id)+","):"")+
    "\"chat_id\":\""+chat_id+"\",\"text\":\"Настройки\",\"reply_markup\":{\"inline_keyboard\":[[{\"text\":\""+mute_name+
    "\",\"callback_data\":\"mute\"},{\"text\":\""+std::string(settings_manager->settings.mute?"🟢 ":"⚫️ ")+
    "\",\"callback_data\":\"mute\"}],[{\"text\":\""+sound_name+
    "\",\"callback_data\":\"sound\"},{\"text\":\""+std::string(settings_manager->settings.sound?"🟢 ":"⚫️ ")+
    "\",\"callback_data\":\"sound\"}],[{\"text\":\""+led_name+
    "\",\"callback_data\":\"led\"},{\"text\":\""+std::string(settings_manager->settings.led?"🟢 ":"⚫️ ")+
    "\",\"callback_data\":\"led\"}],[{\"text\":\""+phone_disable_name+
    "\",\"callback_data\":\"phone_disable\"},{\"text\":\""+std::string(settings_manager->settings.phone_disable?"🟢 ":"⚫️ ")+
    "\",\"callback_data\":\"phone_disable\"}],[{\"text\":\""+std::string(access_code_name) + " " + std::string(settings_manager->settings.access_code==""?"------":settings_manager->settings.access_code)+
    "\",\"callback_data\":\"generate_code\"},{\"text\":\""+access_code_delete_name+
    "\",\"callback_data\":\"delete_code\"}]]}}").c_str());
    return (responce!="") && (responce["ok"].as<bool>());
}

bool TLGManager::sendControlPanel(bool edit, std::string chat_id){
    JsonDocument responce;
    deserializeJson(responce, (char*)post("{\"method\":\""+std::string(edit?"editMessageText":"sendMessage")+
    "\","+std::string(edit?std::string("\"message_id\":"+std::to_string(menu_id)+","):"")+
    "\"chat_id\":\""+chat_id+"\",\"text\":\"Управление\",\"reply_markup\":{\"inline_keyboard\":[[{\"text\":\""+std::string(modes_name)+
    "\",\"callback_data\":\"modes\"},{\"text\":\""+mode_name[settings_manager->settings.modes]+
    "\",\"callback_data\":\"modes\"}],[{\"text\":\""+std::string(accept_call_name)+
    "\",\"callback_data\":\"accept\"},{\"text\":\""+std::string(settings_manager->settings.accept_call?"🟢 ":"⚫️ ")+
    "\",\"callback_data\":\"accept\"}],[{\"text\":\""+std::string(delivery_call_name)+
    "\",\"callback_data\":\"delivery\"},{\"text\":\""+std::string(settings_manager->settings.delivery?"🟢 ":"⚫️ ")+
    "\",\"callback_data\":\"delivery\"}],[{\"text\":\""+std::string(reject_call_name)+
    "\",\"callback_data\":\"reject\"},{\"text\":\""+std::string(settings_manager->settings.reject_call?"🟢 ":"⚫️ ")+
    "\",\"callback_data\":\"reject\"}]]}}").c_str());
    return (responce!="") && (responce["ok"].as<bool>());
}

bool TLGManager::answerCallbackQuery(std::string callback_query_id) {
    JsonDocument responce;
    deserializeJson(responce, (char*)post("{\"method\":\"answerCallbackQuery\",\"callback_query_id\":\""+callback_query_id+"\"}").c_str());
    return (responce!="") && (responce["ok"].as<bool>());
}

bool TLGManager::sendMessage(std::string chat_id, std::string message, bool force, std::string mode) {
    int16_t start = 0;
    int16_t end = chat_id.find(",");
    bool result = chat_id.length() > 0;
    while (end != -1  && result) {
        std::string item_id = chat_id.substr(start, end - start);
        item_id.erase(std::remove(item_id.begin(), item_id.end(), ' '), item_id.end());
        start = end + 1;
        end = chat_id.find(",", start);
        JsonDocument responce;
        deserializeJson(responce, (char*)post("{\"method\":\"sendMessage\",\"chat_id\":\""+item_id+"\",\"text\":\""+message+"\""+  std::string(mode!=""?std::string("\"parse_mode\":\""+mode+"\""):"")+"}", force).c_str());
        result = (responce!="") && (responce["ok"].as<bool>());
    }
    std::string item_id = chat_id.substr(start, end - start);
    item_id.erase(std::remove(item_id.begin(), item_id.end(), ' '), item_id.end());
    JsonDocument responce;
    deserializeJson(responce, (char*)post("{\"method\":\"sendMessage\",\"chat_id\":\""+item_id+"\",\"text\":\""+message+"\""+  std::string(mode!=""?std::string("\"parse_mode\":\""+mode+"\""):"")+"}", force).c_str());
    return result && (responce!="") && (responce["ok"].as<bool>());
}

bool TLGManager::begin() {
    tlg_started = false;
    last_error = 0;
    if (https_client->begin(host.c_str())) {
        https_client->setTimeout(65000);
        https_client->addHeader("Content-Type", "application/json");
        tlg_started = getMe() && setMyCommands();
        if (https_client_post->begin(host.c_str())) { // Пробуем запустить еще один клиент для телеграмм-оповещений.
            https_client_post->addHeader("Content-Type", "application/json");
        } else {  // Если не получилось, то удаляем его нафиг, будем одним работать. Оповещения будут приходить после завершения LongPool update.
            tlg_started = false;
            LOG("[%s] Second connection filed!\n", TAG);
            last_error = 5;
        }
    } else {
        LOG("[%s] Connection filed!\n", TAG);
        last_error = 5;
    }
    if (!tlg_started) {
        LOG("[%s] Telegram authorization failed!\n", TAG);
        last_error = 5;
    } else update_loop = true;
    return tlg_started;
}


void TLGManager::getUpdate() {
    if (!update_loop) return;
    _await = true;
    if (ping) {
        if (millis() - timer > 15000) {
            LOG("[%s] Check ping connection to api.telegram.org #%d\n", TAG, after_ping);
            timer = millis();
            ping = !Ping.ping("api.telegram.org");
            after_ping++;
            if (ping) {
                LOG("[%s] Checking ping connection Failure. Awaiting 15 seconds.\n", TAG);
                if (after_ping > 10) {
                    LOG("[%s] Internet connection lost. System will restart.\n", TAG);
                    ESP.restart();
                }
            } else LOG("[%s] Checking ping connection Success. Reconnect telegram client.\n", TAG);
        }
    } else {
        JsonDocument responce;
        deserializeJson(responce, (char*)post("{\"method\":\"getUpdates\",\"timeout\":60,\"offset\":"+std::to_string(update_id + 1)+"}").c_str());
        if ((responce!="") && (responce["ok"].as<bool>())) {
            last_error = 0;
            after_ping = 0;
            JsonDocument results = responce["result"];
            uint8_t result_count = results.size();
            for (uint8_t r = 0; r < result_count; r++) {
                JsonDocument result = results[r];
                update_id = result["update_id"].as<uint64_t>();
                if (!result["message"].isNull()) {
                    JsonDocument msg = result["message"];
                    message(msg["from"]["id"].as<std::string>(), msg["chat"]["id"].as<std::string>(), msg["text"].as<std::string>(), msg["from"]["username"].as<std::string>());
                }
                if (!result["callback_query"].isNull()) {
                    JsonDocument query = result["callback_query"];
                    std::string id = query["id"].as<std::string>();
                    menu_id = query["message"]["message_id"].as<uint64_t>();
                    uint8_t repeat = 3;
                    callback_query(query["from"]["id"].as<std::string>(), query["message"]["chat"]["id"].as<std::string>(), query["data"].as<std::string>());
                    while (!answerCallbackQuery(id) && repeat--){};
                }
            }
        } else {
            LOG("[%s] GetUpdate failed #%d. Start checking ping connection to api.telegram.org every 15 seconds.\n", TAG, after_ping);
            last_error = 5;
            timer = millis();
            if (after_ping > 10) {
                LOG("[%s] Telegram client incorrect. System will restart.\n", TAG);
                ESP.restart();
            } else {
                https_client->setReuse(false);
                https_client_post->setReuse(false);
                ping = true;
            }
        }
    }
    _await = false;
}