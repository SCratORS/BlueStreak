#pragma once
#define led_status    GPIO_NUM_16        // Индикатор статуса API
#define led_indicator GPIO_NUM_13        // Дополнительный индикатор, который будет показывать режимы и прочее.
#define detect_line   GPIO_NUM_12        // Пин детектора вызова
#define button_boot   GPIO_NUM_0         // Кнопка управления платой и перевода в режим прошивки
#define relay_line    GPIO_NUM_14        // Пин "Переключение линии, плата/трубка"
#define switch_open   GPIO_NUM_17        // Пин "Открытие двери"
#define switch_phone  GPIO_NUM_4         // Пин "Трубка положена/поднята"
#define CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION "BlueStreak™ 2.2.1-Web Insider Preview 04.2025 Firmware"
#define COPYRIGHT "© SCHome (SmartHome Devices), 2025"
#define modes_name "Постоянный режим работы"
#define sound_name "Аудиосообщения"
#define led_name "Светоиндикация"
#define mute_name "Беззвучный режим"
#define phone_disable_name "Отключить трубку"
#define accept_call_name "Открыть дверь"
#define reject_call_name "Сбросить вызов"
#define delivery_call_name "Открыть курьеру"
#define access_code_name "Код открытия: "
#define access_code_delete_name "Удалить код"
#define ACCEPT_FILENAME "/media/access_allowed"
#define GREETING_FILENAME "/media/greeting_allowed"
#define REJECT_FILENAME "/media/access_denied"
#define DELIVERY_FILENAME "/media/delivery_allowed"
#define SETTING_FILENAME "/settings.json"
#define INDEX_FILENAME "/index.html"
#define l_status_call "Вызов"
#define l_status_answer "Ответ"
#define l_status_open "Открытие двери"
#define l_status_reject "Сброс вызова"
#define l_status_close "Закрыто"
#define DISCOVERY_DELAY 500
#define STACK_SIZE 16384
#define CRITICAL_FREE 65536

#include "stdint.h"
#include <string>
#include <FS.h>
#include <ArduinoJson.h>

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
            bool syslog = false;
            bool force_open = false;
            uint16_t syslog_port = 514;
            std::string syslog_server = "";
            uint8_t modes = 0;
            uint8_t server_type = 0;
            uint16_t delay_system = 500;
            uint16_t delay_before = 1000;
            uint16_t delay_open = 600;
            uint16_t delay_after = 3000;
            uint16_t delay_filter  = 10;
            uint16_t call_end_delay  = 6000;
            uint16_t greeting_delay = 1000;
            const uint16_t delay_before_open_door = 100;
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
            std::string dev_name = "smartintercom";
            bool web_auth = false;
            bool child_lock = false;
            bool mqtt_retain = true;
            uint16_t access_code_lifetime = 0;
            uint8_t address_counter = 0;
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
        std::string setDelaySystem(uint16_t value);
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
        std::string setAddressCounter(uint8_t value);
        std::string setSysLog(bool value);
        std::string setSysLogPort(uint16_t value);
        std::string setSysLogServer(std::string value);
        std::string setForceOpen(bool value);
        std::string setDevName(std::string value);

    private:
        JsonDocument json;
        std::string filename;
        std::string message;
};