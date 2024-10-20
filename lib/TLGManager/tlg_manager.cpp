#include "tlg_manager.h"
#include "esp_log.h"
#include "ArduinoJson.h"

static const char * TAG = "TLG";
extern std::string modes_name;
extern std::string mode_name[3];
extern std::string accept_call_name;
extern std::string delivery_call_name;
extern std::string reject_call_name;
extern std::string phone_disable_name;
extern std::string access_code_delete_name;
extern std::string access_code_name;
extern std::string led_name;
extern std::string sound_name;
extern std::string mute_name;

TLGManager::TLGManager(){
    https_client = new HTTPClient();
    https_client_post = new HTTPClient();
    tlg_started = false;
    host = "";
    menu_id = 0;
    update_id = 0;
}

TLGManager::~TLGManager() {
    if (https_client) {
        https_client->end();
        delete(https_client); https_client = nullptr;
    }
    if (https_client_post) {
        https_client_post->end();
        delete(https_client_post); https_client_post = nullptr;
    }          
}

void TLGManager::stop() {
    update_loop = false;
    if (https_client) {
        https_client->setReuse(false);
        https_client->end();
    }
    if (https_client_post) {
        https_client_post->setReuse(false);
        https_client_post->end();
    }
    tlg_started = false;
} 

std::string TLGManager::post(std::string content, bool force) {
    std::string responce = "";
    if (force && !https_client_post) force = false;
    if (force) {   
        update_loop = false;
        ESP_LOGI(TAG, "request force post: %s",content.c_str());
        if (https_client_post) {
            https_client_post->POST(content.c_str());
            responce = https_client_post->getString().c_str();
        }
    } else {
        ESP_LOGI(TAG, "request post: %s",content.c_str());
        if (https_client) {
            https_client->POST(content.c_str());
            responce = https_client->getString().c_str();
        }
    }
    ESP_LOGI(TAG, "post responce: %s", responce.c_str());
    if (force) update_loop = tlg_started;
    return responce;
}

bool TLGManager::getMe() {
    JsonDocument json;
    json["method"] = "getMe";
    std::string request;   
    serializeJson(json, request);
    JsonDocument responce;
    deserializeJson(responce, (char*)post(request).c_str());
    return responce["ok"].as<bool>();
}

bool TLGManager::setMyCommands() {
    JsonDocument json;
    json["method"] = "setMyCommands";
    JsonArray commands = json["commands"].to<JsonArray>();
    JsonDocument cmd_control;
    cmd_control["command"] = "control";
    cmd_control["description"] = "Смена режимов работы";
    JsonDocument cmd_settings;
    cmd_settings["command"] = "settings";
    cmd_settings["description"] = "Изменение настроек устройства";
    commands.add(cmd_control);
    commands.add(cmd_settings);
    std::string request;   
    serializeJson(json, request);
    JsonDocument responce;
    deserializeJson(responce, (char*)post(request).c_str());
    return responce["ok"].as<bool>();   
}

bool TLGManager::sendStartMessage(std::string chat_id) {
    std::string welcome = "SmartIntercom: Телеграм-бот.\nПрошивка: "+std::string(CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION)+"\nПоддержка устройства: https://t.me/smartintercom";
    JsonDocument json;
    json["method"] = "sendMessage";
    json["chat_id"] = chat_id.c_str();
    json["text"] = welcome.c_str();
    JsonObject reply_markup = json["reply_markup"].to<JsonObject>();
    reply_markup["resize_keyboard"] = true;
    JsonArray keyboard = reply_markup["keyboard"].to<JsonArray>();
    JsonArray keyboard_row = keyboard.add<JsonArray>();
    keyboard_row.add("✅ Открой дверь");
    keyboard_row.add("🚚 Открой курьеру");
    keyboard_row.add("🚷 Сбрось вызов");
    std::string request;   
    serializeJson(json, request);
    JsonDocument responce;
    deserializeJson(responce, (char*)post(request).c_str());
    return responce["ok"].as<bool>();
}

bool TLGManager::sendModeKeyboard(bool edit, std::string chat_id) {
    std::string welcome = "Выбор постоянного режима работы:";
    JsonDocument json;
    json["method"] = edit?"editMessageText":"sendMessage";
    if (edit) json["message_id"] = menu_id;
    json["chat_id"] = chat_id.c_str();
    json["text"] = welcome.c_str(); 
    JsonObject reply_markup = json["reply_markup"].to<JsonObject>();
    JsonArray inline_keyboard = reply_markup["inline_keyboard"].to<JsonArray>();

    JsonArray keyboard_row_0 = inline_keyboard.add<JsonArray>();
    JsonDocument mode_0;
    mode_0["text"] = mode_name[0];
    mode_0["callback_data"] = "mode_0";
    keyboard_row_0.add(mode_0);

    JsonArray keyboard_row_1 = inline_keyboard.add<JsonArray>();
    JsonDocument mode_1;
    mode_1["text"] = mode_name[1];
    mode_1["callback_data"] = "mode_1";
    keyboard_row_1.add(mode_1);

    JsonArray keyboard_row_2 = inline_keyboard.add<JsonArray>();
    JsonDocument mode_2;
    mode_2["text"] = mode_name[0];
    mode_2["callback_data"] = "mode_2";
    keyboard_row_2.add(mode_2);

    std::string request;   
    serializeJson(json, request);
    JsonDocument responce;
    deserializeJson(responce, (char*)post(request).c_str());
    return responce["ok"].as<bool>();
 }


bool TLGManager::sendSettingsPanel(bool edit, std::string chat_id){
    std::string welcome = "Настройки";
    JsonDocument json;
    json["method"] = edit?"editMessageText":"sendMessage";
    if (edit) json["message_id"] = menu_id;
    json["chat_id"] = chat_id.c_str();
    json["text"] = welcome.c_str();
    JsonObject reply_markup = json["reply_markup"].to<JsonObject>();
    JsonArray inline_keyboard = reply_markup["inline_keyboard"].to<JsonArray>();

    JsonArray inline_row_0 = inline_keyboard.add<JsonArray>();
    JsonDocument mute_caption;
    mute_caption["text"] = mute_name;
    mute_caption["callback_data"] = "mute";
    inline_row_0.add(mute_caption);
    JsonDocument mute_select;
    mute_select["text"] = settings_manager->settings.mute?"🟢 ":"⚫️ ";
    mute_select["callback_data"] = "mute";
    inline_row_0.add(mute_select);

    JsonArray inline_row_1 = inline_keyboard.add<JsonArray>();
    JsonDocument sound_caption;
    sound_caption["text"] = sound_name;
    sound_caption["callback_data"] = "sound";
    inline_row_1.add(sound_caption);
    JsonDocument sound_select;
    sound_select["text"] = settings_manager->settings.sound?"🟢 ":"⚫️ ";
    sound_select["callback_data"] = "sound";
    inline_row_1.add(sound_select);

    JsonArray inline_row_2 = inline_keyboard.add<JsonArray>();
    JsonDocument led_caption;
    led_caption["text"] = led_name;
    led_caption["callback_data"] = "led";
    inline_row_2.add(led_caption);
    JsonDocument led_select;
    led_select["text"] = settings_manager->settings.led?"🟢 ":"⚫️ ";
    led_select["callback_data"] = "led";
    inline_row_2.add(led_select);

    JsonArray inline_row_3 = inline_keyboard.add<JsonArray>();
    JsonDocument phone_disable_caption;
    phone_disable_caption["text"] = phone_disable_name;
    phone_disable_caption["callback_data"] = "phone_disable";
    inline_row_3.add(phone_disable_caption);
    JsonDocument phone_disable_select;
    phone_disable_select["text"] = settings_manager->settings.phone_disable?"🟢 ":"⚫️ ";
    phone_disable_select["callback_data"] = "phone_disable";
    inline_row_3.add(phone_disable_select);

    JsonArray inline_row_4 = inline_keyboard.add<JsonArray>();
    JsonDocument access_code_caption;
    access_code_caption["text"] = access_code_name + " " + (settings_manager->settings.access_code==""?"------":settings_manager->settings.access_code);
    access_code_caption["callback_data"] = "generate_code";
    inline_row_4.add(access_code_caption);
    JsonDocument access_code_caption_delete;
    access_code_caption_delete["text"] = access_code_delete_name;
    access_code_caption_delete["callback_data"] = "delete_code";
    inline_row_4.add(access_code_caption_delete);

    std::string request;   
    serializeJson(json, request);
    JsonDocument responce;
    deserializeJson(responce, (char*)post(request).c_str());
    return responce["ok"].as<bool>();
}


bool TLGManager::sendControlPanel(bool edit, std::string chat_id){
    std::string welcome = "Управление";
    JsonDocument json;
    json["method"] = edit?"editMessageText":"sendMessage";
    if (edit) json["message_id"] = menu_id;
    json["chat_id"] = chat_id.c_str();
    json["text"] = welcome.c_str();
    JsonObject reply_markup = json["reply_markup"].to<JsonObject>();
    JsonArray inline_keyboard = reply_markup["inline_keyboard"].to<JsonArray>();

    JsonArray inline_row_0 = inline_keyboard.add<JsonArray>();
    JsonDocument modes_caption;
    modes_caption["text"] = modes_name;
    modes_caption["callback_data"] = "modes";
    inline_row_0.add(modes_caption);
    JsonDocument modes_select;
    modes_select["text"] = mode_name[settings_manager->settings.modes];
    modes_select["callback_data"] = "modes";
    inline_row_0.add(modes_select);
   
    JsonArray inline_row_1 = inline_keyboard.add<JsonArray>();
    JsonDocument accept_caption;
    accept_caption["text"] = accept_call_name;
    accept_caption["callback_data"] = "accept";
    inline_row_1.add(accept_caption);
    JsonDocument accept_select;
    accept_select["text"] = settings_manager->settings.accept_call?"🟢 ":"⚫️ ";
    accept_select["callback_data"] = "accept";
    inline_row_1.add(accept_select);

    JsonArray inline_row_2 = inline_keyboard.add<JsonArray>();
    JsonDocument delivery_caption;
    delivery_caption["text"] = delivery_call_name;
    delivery_caption["callback_data"] = "delivery";
    inline_row_2.add(delivery_caption);
    JsonDocument delivery_select;
    delivery_select["text"] = settings_manager->settings.delivery?"🟢 ":"⚫️ ";
    delivery_select["callback_data"] = "delivery";
    inline_row_2.add(delivery_select);

    JsonArray inline_row_3 = inline_keyboard.add<JsonArray>();
    JsonDocument reject_caption;
    reject_caption["text"] = reject_call_name;
    reject_caption["callback_data"] = "reject";
    inline_row_3.add(reject_caption);
    JsonDocument reject_select;
    reject_select["text"] = settings_manager->settings.reject_call?"🟢 ":"⚫️ ";
    reject_select["callback_data"] = "reject";
    inline_row_3.add(reject_select);

    std::string request;   
    serializeJson(json, request);
    JsonDocument responce;
    deserializeJson(responce, (char*)post(request).c_str());
    return responce["ok"].as<bool>();
}

bool TLGManager::answerCallbackQuery(std::string callback_query_id) {
    JsonDocument json;
    json["method"] = "answerCallbackQuery";
    json["callback_query_id"] = callback_query_id.c_str();
    std::string request;   
    serializeJson(json, request);
    JsonDocument responce;
    deserializeJson(responce, (char*)post(request).c_str());
    return responce["ok"].as<bool>();
}

bool TLGManager::sendMessage(std::string chat_id, std::string message, bool force, std::string mode) {
    JsonDocument json;
    json["method"] = "sendMessage";
    json["chat_id"] = chat_id.c_str();
    json["text"] = message.c_str();
    if (mode != "") json["parse_mode"] = mode.c_str();
    std::string request;
    serializeJson(json, request);
    JsonDocument responce;
    deserializeJson(responce, (char*)post(request, force).c_str());
    return responce["ok"].as<bool>();
}

bool TLGManager::begin() {
    tlg_started = false;
    if (https_client && https_client->begin(host.c_str())) {
        https_client->setTimeout(65000);
        https_client->addHeader("Content-Type", "application/json");
        tlg_started = getMe() && setMyCommands();
        if (https_client_post->begin(host.c_str())) { // Пробуем запустить еще один клиент для телеграмм-оповещений.
            https_client_post->addHeader("Content-Type", "application/json");
        } else {  // Если не получилось, то удаляем его нафиг, будем одним работать. Оповещения будут приходить после завершения LongPool update.
            delete(https_client_post); https_client_post = nullptr;
        }
    } else {
        ESP_LOGI(TAG, "Connection filed!");
        last_error = 5;
    }
    if (!tlg_started) {
        ESP_LOGI(TAG, "Telegram authorization failed!");
        last_error = 5;
    } else update_loop = true;
    return tlg_started;
}

void TLGManager::getUpdate() {
    if (!update_loop) return;
    JsonDocument json;
    json["method"] = "getUpdates";
    json["timeout"] = 60;
    json["offset"] = update_id + 1;
    std::string request;   
    serializeJson(json, request);
    JsonDocument responce;
    deserializeJson(responce, (char*)post(request).c_str());
    if (responce["ok"].as<bool>()) {
        last_error = 0;
        JsonDocument results = responce["result"];
        uint8_t result_count = results.size();
        for (uint8_t r = 0; r < result_count; r++) {
            JsonDocument result = results[r];
            update_id = result["update_id"].as<uint64_t>();
            if (!result["message"].isNull()) {
                JsonDocument msg = result["message"];
                std::string from_id = msg["from"]["id"].as<std::string>();
                std::string chat_id = msg["chat"]["id"].as<std::string>();
                std::string text = msg["text"].as<std::string>();
                message(from_id, chat_id, text);
            }
            if (!result["callback_query"].isNull()) {
                JsonDocument query = result["callback_query"];
                std::string id = query["id"].as<std::string>();
                std::string from_id = query["from"]["id"].as<std::string>();
                std::string chat_id = query["message"]["chat"]["id"].as<std::string>();
                std::string data = query["data"].as<std::string>();
                menu_id = query["message"]["message_id"].as<uint64_t>();
                uint8_t repeat = 3;
                callback_query(from_id, chat_id, data);
                while (!answerCallbackQuery(id) && repeat--){};
            }
        }
    } else last_error = 5;

}