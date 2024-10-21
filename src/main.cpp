#define led_status    GPIO_NUM_16        // Индикатор статуса API
#define led_indicator GPIO_NUM_13        // Дополнительный индикатор, который будет показывать режимы и прочее.
#define detect_line   GPIO_NUM_12        // Пин детектора вызова
#define button_boot   GPIO_NUM_0         // Кнопка управления платой и перевода в режим прошивки
#define relay_line    GPIO_NUM_14        // Пин "Переключение линии, плата/трубка"
#define switch_open   GPIO_NUM_17        // Пин "Открытие двери"
#define switch_phone  GPIO_NUM_4         // Пин "Трубка положена/поднята"
#define ACCEPT_FILENAME "/media/access_allowed.mp3"
#define GREETING_FILENAME "/media/greeting_allowed.mp3"
#define REJECT_FILENAME "/media/access_denied.mp3"
#define DELIVERY_FILENAME "/media/delivery_allowed.mp3"
#define SETTING_FILENAME "/settings.json"
#define INDEX_FILENAME "/index.html"
#define l_status_call "Вызов"
#define l_status_answer "Ответ"
#define l_status_open "Открытие двери"
#define l_status_reject "Сброс вызова"
#define l_status_close "Закрыто"
#define STACK_SIZE 8192
#define CRITICAL_FREE 65536

#include <ESPAsyncWebserver.h>
#include <Update.h>
#include "FTPServer.h"
#include "soc/rtc_wdt.h"
#include <ArduinoJson.h>
#include "wifi_manager.h"
#include "settings_manager.h"
#include "mqtt_manager.h"
#include "tlg_manager.h"
#include "esp_random.h"

#include "entity.h"
#include "switch.h"
#include "select.h"
#include "sensor.h"
#include "binary_sensor.h"

#if defined(ESP32) && !defined(USE_ESP32_VARIANT_ESP32C3)
    #include "AudioOutputI2S.h"
    using aAudioOutput = AudioOutputI2S;
    #define _AudioOutput() aAudioOutput(0, aAudioOutput::INTERNAL_DAC)
#else
    #include "AudioOutputI2SNoDAC.h"
    using aAudioOutput = AudioOutputI2SNoDAC;
    #define _AudioOutput() aAudioOutput()
#endif
#if defined(SDCARD)
    #include "SD.h"
    #include "AudioFileSourceSD.h"
    #define aFS SD
    #define aFS_STR "SD"
    using aAudioFileSource = AudioFileSourceSD;
#else
    #include "LittleFS.h"
    #include "AudioFileSourceLittleFS.h"
    #define aFS LittleFS
    #define aFS_STR "LittleFS"
    using aAudioFileSource = AudioFileSourceLittleFS;
#endif
#include "AudioGeneratorMP3.h"

static const char* TAG = "MAIN";
std::string mode_name[3] = {"Не активен","Сброс вызова","Открывать всегда"}; 
std::string modes_name = "Постоянный режим работы";
std::string sound_name = "Аудиосообщения";
std::string led_name = "Светоиндикация";
std::string mute_name = "Беззвучный режим";
std::string phone_disable_name = "Отключить трубку";
std::string accept_call_name = "Открыть дверь";
std::string reject_call_name = "Сбросить вызов";
std::string delivery_call_name = "Открыть курьеру";
std::string access_code_name = "Код открытия: ";
std::string access_code_delete_name = "Удалить код";
enum {WAIT, CALLING, CALL, SWUP, VOICE, PREOPEN, SWOPEN, GREETING, GREETING_VOICE, DROP, ENDING, RESET};
uint8_t currentAction = WAIT;
uint32_t detectMillis = 0;
uint32_t audioLength = 0;

aAudioOutput *audioOut = new _AudioOutput();
AudioGeneratorMP3 *audioPlayer;
AudioFileSource *audioFile;

const uint16_t DEBOUNCE_DELAY = 100;
const uint16_t LONGPRESS_DELAY = 5000;
bool btnPressFlag = false;
uint32_t last_toggle;

struct {
  bool line_detect;
  std::string line_status;
} device_status;

struct {
  bool web_services_init = false;
  bool time_configure = false;
  uint8_t last_error = 0;
} hw_status;

FTPServer * ftp_server;
MQTTManager * mqtt_manager;
TLGManager * tlg_manager;
WiFiManager * wifi_manager;
SettingsManager * settings_manager;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

Switch * accept_once;
Switch * reject_once;
Switch * delivery_once;
Switch * sound;
Switch * led;
Switch * mute;
Switch * phone_disable;
BinarySensor * line_detect;
Sensor * line_status;
Select * modes;

DevInfo * device_info;

hw_timer_t * timer0 = NULL;
void ICACHE_RAM_ATTR call_detector_enable();

uint8_t ledIndicatorCounter = 0;
uint8_t ledStatusCounter = 0;
uint8_t ledErrorCounter = 0;

void mqtt_publish_once_actions(){
  if (accept_once) accept_once->publishValue();
  if (delivery_once) delivery_once->publishValue();
  if (reject_once) reject_once->publishValue();
}

std::string get_fs_used() {
  const char *sizes[5] = { "B", "KB", "MB", "GB", "TB" };
  int64_t  get_fs_used = aFS.totalBytes() - aFS.usedBytes();
  double base = log(max(get_fs_used - CRITICAL_FREE, (int64_t) 0)) / log(1024);
  uint8_t b_hi = round(base);
  char str[10] = "";
  sprintf(str, "%.2f %s", pow(1024, base - b_hi), sizes[b_hi]);
  return std::string(str);
}

void setLineStatus(std::string value){
  device_status.line_status = value;
  JsonDocument json;
  json["line_status"] = device_status.line_status;
  std::string message;
  serializeJson(json, message);
  ESP_LOGI (TAG, "%s", message.c_str());
  if (line_status) line_status->publishValue();
  ws.textAll(message.c_str());
}

void setLineDetect(bool value){
  device_status.line_detect = value;
  JsonDocument json;
  json["line_detect"] = device_status.line_detect;
  std::string message;
  serializeJson(json, message);
  ESP_LOGI (TAG, "%s", message.c_str());
  if (line_detect) line_detect->publishValue();
  if (tlg_manager) {
    if (settings_manager->settings.delivery) tlg_manager->sendMessage(settings_manager->settings.tlg_user, "🚚 Входящий вызов в домофон!\nОткрываю дверь один раз.", true);
    else if (settings_manager->settings.accept_call) tlg_manager->sendMessage(settings_manager->settings.tlg_user, "✅ Входящий вызов в домофон!\nОткрываю дверь один раз.", true);
    else if (settings_manager->settings.reject_call) tlg_manager->sendMessage(settings_manager->settings.tlg_user, "🚷 Входящий вызов в домофон!\nСбрасываю вызов.", true);
    else {
      switch (settings_manager->settings.modes) {
        case 0: tlg_manager->sendMessage(settings_manager->settings.tlg_user, "🛎 Входящий вызов в домофон!", true); break;
        case 1: tlg_manager->sendMessage(settings_manager->settings.tlg_user, "🚷 Входящий вызов в домофон!\nСбрасываю вызов.", true); break;
        case 2: tlg_manager->sendMessage(settings_manager->settings.tlg_user, "✅ Входящий вызов в домофон!\nОткрываю дверь.", true); break;
      }
    }
  }
  ws.textAll(message.c_str());
}

void sendAlert(std::string value){
  JsonDocument json;
  json["alert"] = value;
  std::string message;
  serializeJson(json, message);
  ESP_LOGI (TAG, "%s", message.c_str());
  ws.textAll(message.c_str());
}

void sendStatus(){
  JsonDocument json;
  json["accept_call"] = settings_manager->settings.accept_call;
  json["delivery"] =    settings_manager->settings.delivery;
  json["reject_call"] = settings_manager->settings.reject_call;
  json["line_detect"] = device_status.line_detect;
  json["line_status"] = device_status.line_status;
  std::string message;
  serializeJson(json, message);
  ESP_LOGI (TAG, "%s", message.c_str());
  ws.textAll(message.c_str());
  if (line_detect) line_detect->publishValue();
  if (line_status) line_status->publishValue();
  mqtt_publish_once_actions();
}

std::string getMediaExists() {
  JsonDocument json;
  json["fs_used"] = get_fs_used();
  json["access_allowed_play"] = aFS.exists(ACCEPT_FILENAME)?ACCEPT_FILENAME:NULL;
  json["greeting_allowed_play"] =    aFS.exists(GREETING_FILENAME)?GREETING_FILENAME:NULL;
  json["delivery_allowed_play"] = aFS.exists(DELIVERY_FILENAME)?DELIVERY_FILENAME:NULL;
  json["access_denied_play"] =    aFS.exists(REJECT_FILENAME)?REJECT_FILENAME:NULL;

  std::string message;
  serializeJson(json, message);
  ESP_LOGI (TAG, "%s", message.c_str());
  return message;
}

std::string getStatus(){ 
  device_info->control = "http://"+wifi_manager->ip;
  JsonDocument json;
  json["ftp"] = (ftp_server)?true:false;
  json["ip"] = wifi_manager->ip;
  json["line_detect"] = device_status.line_detect;
  json["line_status"] = device_status.line_status;
  json["firmware"] = CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION;
  json["copyright"] = COPYRIGHT;
  std::string message;
  serializeJson(json, message);
  ESP_LOGI (TAG, "%s", message.c_str());
  return message;
}

/*
0 - Ошибок нет.
1 - Отсутствует соединение с WiFi
2 - Активна WiFi СофтТД
3 - Не удалось загрузить настройки из файла.
4 - Не удалось сохранить настройки в файл.
5 - Не удалось подключиться к серверу взаимодействия
6 - Не удалось проинициализировать файловую систему. Критичная ошибка.
*/
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
void IRAM_ATTR TimerHandler0() {
  portENTER_CRITICAL_ISR(&timerMux);
  if (hw_status.last_error) {
    if (ledErrorCounter++ < hw_status.last_error * 10) {
      gpio_set_level(led_status, (ledErrorCounter%10 < 5)?1:0);
      ledcWrite(0, (ledErrorCounter%10 < 5)?40:1);
    }
    if (ledErrorCounter>160) ledErrorCounter = 0;
  } else if (settings_manager && settings_manager->last_error) {
    if (ledErrorCounter++ < settings_manager->last_error * 10) {
      gpio_set_level(led_status, (ledErrorCounter%10 < 5)?1:0);
      ledcWrite(0, (ledErrorCounter%10 < 5)?40:0);
    }
    if (ledErrorCounter>160) ledErrorCounter = 0;
  } else if (wifi_manager && wifi_manager->last_error) {
    if (ledErrorCounter++ < wifi_manager->last_error * 10) {
      gpio_set_level(led_status, (ledErrorCounter%10 < 5)?1:0);
      ledcWrite(0, (ledErrorCounter%10 < 5)?40:0);
    }
    if (ledErrorCounter>160) ledErrorCounter = 0;
  } else if (mqtt_manager && mqtt_manager->last_error) {
    if (ledErrorCounter++ < mqtt_manager->last_error * 10) {
      gpio_set_level(led_status, (ledErrorCounter%10 < 5)?1:0);
      ledcWrite(0, (ledErrorCounter%10 < 5)?40:0);
    }
    if (ledErrorCounter>160) ledErrorCounter = 0;
  } else if (tlg_manager && tlg_manager->last_error) {
    if (ledErrorCounter++ < tlg_manager->last_error * 10) {
      gpio_set_level(led_status, (ledErrorCounter%10 < 5)?1:0);
      ledcWrite(0, (ledErrorCounter%10 < 5)?40:0);
    }
    if (ledErrorCounter>160) ledErrorCounter = 0;
  } else {
    if (settings_manager && settings_manager->settings.led) {

      if (settings_manager->settings.reject_call) {
          switch (ledStatusCounter++) {
              case 0: gpio_set_level(led_status, 1); break;
              case 1: gpio_set_level(led_status, 0); break;
              case 40: ledStatusCounter = 0; break;
              default: if (ledStatusCounter > 40) ledStatusCounter = 0;
          }
      } else gpio_set_level(led_status, 0);

      if (currentAction != WAIT) {
        switch (ledIndicatorCounter++) {
            case 0: ledcWrite(0, 240); break;
            case 1: ledcWrite(0, 0); ledIndicatorCounter = 0;break;
            default: if (ledIndicatorCounter > 1) ledIndicatorCounter = 0; break;
        }
      } else if (settings_manager->settings.accept_call) {
        switch (ledIndicatorCounter++) {
            case 0: ledcWrite(0, 240); break;
            case 2: ledcWrite(0, 0); break;
            case 40: ledIndicatorCounter = 0; break;
            default: if (ledIndicatorCounter > 40) ledIndicatorCounter = 0; break;
        }
      } else if (settings_manager->settings.mute) {
        ledIndicatorCounter++;
        if (ledIndicatorCounter < 40) {
            ledcWrite(0, ledIndicatorCounter * 6);
        } else if (ledIndicatorCounter < 80) {
            ledcWrite(0, (80 - ledIndicatorCounter) * 6);
        } else ledIndicatorCounter = 0;
      } else ledcWrite(0, 0);
    } else {
      gpio_set_level(led_status, 0);
      ledcWrite(0, 0);
    }
  }
  portEXIT_CRITICAL_ISR(&timerMux);
}

uint64_t reset_time = 0;
void call_detector_enable() {
  reset_time = millis();
  if (currentAction == WAIT) {
    if (settings_manager->settings.mute) {
      gpio_set_level(switch_phone, 1);  
      gpio_set_level(relay_line, 1);
    }
    detectMillis = millis();
    currentAction = CALLING;
  }
}

void phone_disable_action () {
    if (currentAction == WAIT) {
      gpio_set_level(switch_phone, settings_manager->settings.phone_disable); 
      gpio_set_level(relay_line, settings_manager->settings.phone_disable);
    }
}

uint64_t timerAction = 0;
void doAction(uint32_t timer) {
  switch (currentAction) {
    case WAIT:  break;
    case CALLING: if (millis() - reset_time > settings_manager->settings.call_end_delay) currentAction = RESET;
                  if (timer > settings_manager->settings.delay_filter && !gpio_get_level(detect_line)) {
                    if (!device_status.line_detect) {
                      setLineDetect(true);
                      setLineStatus(l_status_call);
                    }
                    if (settings_manager->settings.accept_call ||
                        settings_manager->settings.delivery ||
                        settings_manager->settings.reject_call ||
                        settings_manager->settings.modes ) {
                          currentAction = CALL;
                          detectMillis = millis(); 
                          timerAction += settings_manager->settings.delay_before;       
                        }    
                  }
                  break;
    case CALL:  if (timer > timerAction) {
                  gpio_set_level(switch_phone, 0);  
                  gpio_set_level(relay_line, 1);
                  setLineStatus(l_status_answer);
                  timerAction += settings_manager->settings.delay_before;
                  currentAction = SWUP;
                } break;
    case SWUP:  if (timer > timerAction) {
                  if (settings_manager->settings.sound || settings_manager->settings.delivery) {
                      audioFile = new aAudioFileSource(settings_manager->settings.delivery ? DELIVERY_FILENAME : 
                                                            settings_manager->settings.accept_call ? ACCEPT_FILENAME :
                                                            settings_manager->settings.reject_call ? REJECT_FILENAME :
                                                            settings_manager->settings.modes == 2 ? ACCEPT_FILENAME : REJECT_FILENAME);
                    if (audioFile) {
                      audioPlayer = new AudioGeneratorMP3();
                      audioPlayer->begin(audioFile, audioOut);
                      audioLength = millis();
                      currentAction = VOICE;
                      break;
                    }
                  }
                  audioLength = 0;
                  currentAction = ( settings_manager->settings.delivery || 
                                    settings_manager->settings.accept_call || 
                                    (settings_manager->settings.modes == 2 && !settings_manager->settings.reject_call)) ? PREOPEN : DROP;
                  if (currentAction == PREOPEN) timerAction += settings_manager->settings.delay_before;
                } break;
    case VOICE: if (!audioPlayer->loop()) {
                  audioPlayer->stop();
                  audioLength = millis() - audioLength;
                  delete audioPlayer; audioPlayer = nullptr;
                  delete audioFile; audioFile = nullptr;
                  currentAction = ( settings_manager->settings.delivery || 
                                    settings_manager->settings.accept_call || 
                                    (settings_manager->settings.modes == 2 && !settings_manager->settings.reject_call)) ? SWOPEN : DROP;
                } break;
    case PREOPEN: if (timer > timerAction) currentAction = SWOPEN;
                  break;
    case SWOPEN:  gpio_set_level(switch_open, 1);
                  setLineStatus(l_status_open);
                  timerAction += (audioLength + settings_manager->settings.delay_open);
                  currentAction = settings_manager->settings.greeting?GREETING:DROP;
                  break;
    case GREETING: if (timer > timerAction) {
                    if (settings_manager->settings.sound) {
                      audioFile = new aAudioFileSource(GREETING_FILENAME);
                      if (audioFile) {
                        setLineStatus(l_status_answer);
                        audioPlayer = new AudioGeneratorMP3();
                        audioPlayer->begin(audioFile, audioOut);
                        audioLength = millis();
                        currentAction = GREETING_VOICE;
                        gpio_set_level(switch_open, 0);
                        timerAction += settings_manager->settings.greeting_delay;
                        break;
                      }
                    }
                    currentAction = DROP;
                  } break;
    case GREETING_VOICE: 
                if (timer > timerAction) {
                  if (!audioPlayer->loop()) {
                    audioPlayer->stop();
                    audioLength = millis() - audioLength;
                    delete audioPlayer; audioPlayer = nullptr;
                    delete audioFile; audioFile = nullptr;
                    timerAction += audioLength;
                    currentAction = DROP;
                  }
                }
                break;
    case DROP:  if (timer > timerAction) {
                  gpio_set_level(switch_open, 0);
                  gpio_set_level(switch_phone, 1); 
                  setLineStatus(l_status_reject);
                  currentAction = ENDING;
                  timerAction += settings_manager->settings.delay_after;
                } break;
    case ENDING:if (timer > timerAction) {
                  currentAction = RESET;
                } break;
    case RESET: gpio_set_level(relay_line, settings_manager->settings.phone_disable);
                gpio_set_level(switch_phone, settings_manager->settings.phone_disable); 
                gpio_set_level(switch_open, 0);
                settings_manager->settings.accept_call = false;
                settings_manager->settings.delivery = false;
                settings_manager->settings.reject_call = false;
                device_status.line_detect = false;
                device_status.line_status = l_status_close;
                sendStatus();
                currentAction = WAIT;
                timerAction = 0;
                break;
  }
}

std::string enable_ftp_server(bool value) {
  if (value) {
    if (!ftp_server) ftp_server = new FTPServer(aFS);
    ftp_server->begin("", "");
    settings_manager->settings.ftp = true;
  } else {
    settings_manager->settings.ftp = false;
    if (ftp_server) {
      ftp_server->stop();
      delete ftp_server; ftp_server = nullptr;
    }
  }
  JsonDocument json;
  json["ftp"] = value;
  std::string message;
  serializeJson(json, message);
  ESP_LOGI (TAG, "%s", message.c_str());
  return message;
}

void setMode(uint8_t value) {
  ws.textAll(settings_manager->setMode(value).c_str());
  if (modes) modes->publishValue();
}

void setAccept(bool value) {
  ws.textAll(settings_manager->setAccept(value).c_str());
  mqtt_publish_once_actions();
}

void setDelivery(bool value) {
  ws.textAll(settings_manager->setDelivery(value).c_str());
  mqtt_publish_once_actions();
}

void setReject(bool value) {
  ws.textAll(settings_manager->setReject(value).c_str());
  mqtt_publish_once_actions();
}

void setLed(bool value) {
  ws.textAll(settings_manager->setLed(value).c_str());
  if (led) led->publishValue();
}

void setMute(bool value) {
  ws.textAll(settings_manager->setMute(value).c_str());
  if (mute) mute->publishValue();
}

void setPhoneDisable(bool value) {
  ws.textAll(settings_manager->setPhoneDisable(value).c_str());
  if (phone_disable) phone_disable->publishValue();
  phone_disable_action();
}

void setSound(bool value) {
  ws.textAll(settings_manager->setSound(value).c_str());
  if (sound) sound->publishValue();
}

void tlg_code_generate(){
    std::string code = "";
    esp_random();
    for (uint8_t i = 0; i<6; i++)
    code = code + std::to_string(esp_random())[0];
    ws.textAll(settings_manager->setTLGCode(code).c_str());
    settings_manager->access_code_expires = 0;
    tlg_manager->deletePublicAccess();
    settings_manager->SaveSettings(aFS);
}
void tlg_code_delete(){
    ws.textAll(settings_manager->setTLGCode("").c_str());
    settings_manager->access_code_expires = 0;
    tlg_manager->deletePublicAccess();
    settings_manager->SaveSettings(aFS);
}

void tlg_message(std::string from_id, std::string chat_id, std::string message, std::string user_name) {
  if (settings_manager->settings.tlg_user.find(from_id) == std::string::npos) {
      //public message 
      if (message == "/start") tlg_manager->sendMessage(chat_id, "🏠 Введите код доступа.");
      else if (settings_manager->settings.access_code != "" && settings_manager->settings.access_code == message) {
        if (settings_manager->settings.access_code_lifetime && !settings_manager->access_code_expires) {
          settings_manager->access_code_expires = millis()+(settings_manager->settings.access_code_lifetime * 60000);
          std::string lifetime = std::to_string(settings_manager->settings.access_code_lifetime);
          tlg_manager->sendOpenKeyboard(chat_id, "⚠️ Код успешно активирован.\n🕓 Срок действия кода: " + lifetime + " мин.\n✅ Используйте клавиатуру снизу для открытия двери.\n✉️ Введите текст сообщения для отправки его Администратору.");
          tlg_manager->addPublicChatId(from_id);
          tlg_manager->sendMessage(settings_manager->settings.tlg_user, "✅ Код активирован пользователем @"+user_name);
        } else {
          if (!settings_manager->access_code_expires) {
            if (!settings_manager->settings.access_code_lifetime) {
              tlg_manager->sendOpenKeyboard(chat_id, "⚠️ Код принят.\n✅ Используйте клавиатуру снизу для открытия двери.\n✉️ Введите текст сообщения для отправки его Администратору.");
              tlg_manager->addPublicChatId(from_id);
              tlg_manager->sendMessage(settings_manager->settings.tlg_user, "✅ Код активирован пользователем @"+user_name);
            }
          } else {
            std::string lifetime = std::to_string((settings_manager->access_code_expires - millis()) / 60000);
            tlg_manager->sendOpenKeyboard(chat_id, "⚠️ Код принят.\n🕓 Срок действия кода: " + lifetime + " мин.\n✅ Используйте клавиатуру снизу для открытия двери\n✉️ Введите текст сообщения для отправки его Администратору.");
            tlg_manager->addPublicChatId(from_id);
            tlg_manager->sendMessage(settings_manager->settings.tlg_user, "✅ Код активирован пользователем @"+user_name);
          }
        }
      } else if (tlg_manager->public_chat_id.find(from_id) == std::string::npos) {
          if (message == "✅ Открой дверь") tlg_manager->deletePublicAccess(chat_id);
          else tlg_manager->sendMessage(chat_id, "⛔️ Доступ запрещён.");          
          if (settings_manager->settings.access_code == "") tlg_manager->sendMessage(settings_manager->settings.tlg_user, "❗️Попытка ввода кода или сообщение.\nПользователь: @" + user_name + "\nТекст:\n" + message);
      } else {
          if (message == "✅ Открой дверь") {setAccept(true); tlg_manager->sendMessage(chat_id, "Открываю дверь.");}
          else {
            tlg_manager->sendMessage(settings_manager->settings.tlg_user, "❗️Cообщение от пользователя @" + user_name + "\nТекст:\n" + message);
            tlg_manager->sendMessage(chat_id, "Ваше сообщение отправлено Администратору.");
          }
      }
    } else {
      //private message 
      if (message == "/start") tlg_manager->sendStartMessage(chat_id);
      else if (message == "/control") tlg_manager->sendControlPanel(false, chat_id);
      else if (message == "/settings") tlg_manager->sendSettingsPanel(false, chat_id);
      else if (message == "✅ Открой дверь") {setAccept(true); tlg_manager->sendMessage(chat_id, "Открываю дверь.");}
      else if (message == "🚚 Открой курьеру") {setDelivery(true); tlg_manager->sendMessage(chat_id, "Открываю дверь курьеру.");}
      else if (message == "🚷 Сбрось вызов") {setReject(true); tlg_manager->sendMessage(chat_id, "Сбрасываю вызов.");}
      else if (settings_manager->settings.access_code != "" && settings_manager->settings.access_code == message) {
        if (!settings_manager->access_code_expires) {
          if (settings_manager->settings.access_code_lifetime) tlg_manager->sendMessage(chat_id, "🔐 Код не активирован.");
          else tlg_manager->sendMessage(chat_id, "🔐 Код активен без срока действия.");
        } else {
          std::string lifetime = std::to_string((settings_manager->access_code_expires - millis()) / 60000);
          tlg_manager->sendMessage(chat_id, "🕓 До окончания срока действия кода осталось: " + lifetime + " мин.");
        }
      }
    }
}

void tlg_callback_query(std::string from_id, std::string chat_id, std::string callback_query) {
  if (settings_manager->settings.tlg_user.find(from_id) == std::string::npos) return; // Только приватный доступ
  if (callback_query == "") return; // Пустой запрос

  if (callback_query == "modes") tlg_manager->sendModeKeyboard(true, chat_id);
  else if (callback_query == "generate_code") {
    tlg_code_generate();
    tlg_manager->sendSettingsPanel(true, chat_id);
    tlg_manager->sendMessage(chat_id, "`" + settings_manager->settings.access_code + "`", false, "MarkdownV2");
  } 
  else if (callback_query == "mode_0") { setMode(0); tlg_manager->sendControlPanel(true, chat_id);}
  else if (callback_query == "mode_1") { setMode(1); tlg_manager->sendControlPanel(true, chat_id);}
  else if (callback_query == "mode_2") { setMode(2); tlg_manager->sendControlPanel(true, chat_id);}
  else if (callback_query == "accept") { setAccept(!settings_manager->settings.accept_call); tlg_manager->sendControlPanel(true, chat_id);}
  else if (callback_query == "delivery") { setDelivery(!settings_manager->settings.delivery); tlg_manager->sendControlPanel(true, chat_id);}
  else if (callback_query == "reject") { setReject(!settings_manager->settings.reject_call); tlg_manager->sendControlPanel(true, chat_id);}
  else if (callback_query == "mute") { setMute(!settings_manager->settings.mute); tlg_manager->sendSettingsPanel(true, chat_id);}
  else if (callback_query == "sound") { setSound(!settings_manager->settings.sound); tlg_manager->sendSettingsPanel(true, chat_id);}
  else if (callback_query == "led") { setLed(!settings_manager->settings.led); tlg_manager->sendSettingsPanel(true, chat_id);}
  else if (callback_query == "phone_disable") { setPhoneDisable(!settings_manager->settings.phone_disable); tlg_manager->sendSettingsPanel(true, chat_id);}
  else if (callback_query == "delete_code") { tlg_code_delete(); tlg_manager->sendSettingsPanel(true, chat_id); }
}

void mqtt_callback(char* topic, uint8_t* payload, uint32_t length) {
  payload[length] = 0;
  std::string strTopic = (char*)topic;
  std::string message  = (char*)payload;
  ESP_LOGI(TAG, "MQTT TOPIC: %s MESSAGE: %s", strTopic.c_str(), message.c_str());
  mqtt_manager->getEntity(strTopic.c_str())->callback(message);
  
  if (strTopic == modes->callback_topic) setMode(settings_manager->settings.modes);
  else if (strTopic == led->callback_topic) setLed(settings_manager->settings.led); 
  else if (strTopic == sound->callback_topic) setSound(settings_manager->settings.sound); 
  else if (strTopic == phone_disable->callback_topic) setPhoneDisable(settings_manager->settings.phone_disable);
  else if (strTopic == mute->callback_topic) setMute(settings_manager->settings.mute);
  else if (strTopic == accept_once->callback_topic) setAccept(settings_manager->settings.accept_call);
  else if (strTopic == delivery_once->callback_topic) setDelivery(settings_manager->settings.delivery);
  else if (strTopic == reject_once->callback_topic) setReject(settings_manager->settings.reject_call);
}

void entity_configuration(PubSubClient * mqtt_client) {
  accept_once = new Switch("accept_call", mqtt_client, &settings_manager->settings.accept_call, &settings_manager->settings.mqtt_retain);
  reject_once = new Switch("reject_call", mqtt_client, &settings_manager->settings.reject_call, &settings_manager->settings.mqtt_retain);
  delivery_once = new Switch("delivery_call", mqtt_client, &settings_manager->settings.delivery, &settings_manager->settings.mqtt_retain);
  sound = new Switch("sound", mqtt_client, &settings_manager->settings.sound, &settings_manager->settings.mqtt_retain);
  led = new Switch("led", mqtt_client, &settings_manager->settings.led, &settings_manager->settings.mqtt_retain);
  mute = new Switch("mute", mqtt_client, &settings_manager->settings.mute, &settings_manager->settings.mqtt_retain);
  phone_disable = new Switch("phone_disable", mqtt_client, &settings_manager->settings.phone_disable, &settings_manager->settings.mqtt_retain);
  line_detect = new BinarySensor("line_detect", mqtt_client, &device_status.line_detect, &settings_manager->settings.mqtt_retain);
  line_status = new Sensor("line_status", mqtt_client, &device_status.line_status, &settings_manager->settings.mqtt_retain);
  modes = new Select("modes", mqtt_client, &settings_manager->settings.modes, &settings_manager->settings.mqtt_retain);

  modes->items_list.insert(std::make_pair(0, mode_name[0]));
  modes->items_list.insert(std::make_pair(1, mode_name[1]));
  modes->items_list.insert(std::make_pair(2, mode_name[2]));
  modes->ic = "mdi:deskphone";modes->friendly_name = modes_name; mqtt_manager->addEntity(modes);

  line_status->ic = "mdi:bell";line_status->friendly_name = "Статус линии"; mqtt_manager->addEntity(line_status);
  accept_once->ic = "mdi:door-open";accept_once->friendly_name = accept_call_name; mqtt_manager->addEntity(accept_once);
  reject_once->ic = "mdi:phone-hangup";reject_once->friendly_name = reject_call_name; mqtt_manager->addEntity(reject_once);
  delivery_once->ic = "mdi:package";delivery_once->friendly_name = delivery_call_name; mqtt_manager->addEntity(delivery_once);
  line_detect->friendly_name = "Детектор вызова"; mqtt_manager->addEntity(line_detect);
  sound->ent_cat = "config";sound->ic = "mdi:volume-high";sound->friendly_name = sound_name; mqtt_manager->addEntity(sound);
  led->ent_cat = "config";led->ic = "mdi:led-on";led->friendly_name = led_name; mqtt_manager->addEntity(led);
  mute->ent_cat = "config";mute->ic = "mdi:bell-off";mute->friendly_name = mute_name; mqtt_manager->addEntity(mute);
  phone_disable->ent_cat = "config";phone_disable->ic = "mdi:phone-off";phone_disable->friendly_name = phone_disable_name; mqtt_manager->addEntity(phone_disable);
}

void entity_delete() {
  delete(accept_once);accept_once = nullptr;
  delete(reject_once);reject_once = nullptr;
  delete(delivery_once);delivery_once = nullptr;
  delete(sound);sound = nullptr;
  delete(led);led = nullptr;
  delete(mute);mute = nullptr;
  delete(phone_disable);phone_disable = nullptr;
  delete(line_detect);line_detect = nullptr;
  delete(line_status);line_status = nullptr;
  delete(modes);modes = nullptr;
}


TaskHandle_t getTLGUpdateTask;
void getTLGUpdate(void * pvParameters) {
  while (tlg_manager && tlg_manager->enabled()) {
    tlg_manager->getUpdate();
  }
  delete (tlg_manager); tlg_manager = nullptr;
  vTaskDelete(getTLGUpdateTask);
}


void enable_tlg(bool value){
  if (value) {
    if (tlg_manager) return;
    tlg_manager = new TLGManager();
    tlg_manager->settings_manager = settings_manager;
    tlg_manager->setToken(settings_manager->settings.tlg_token);
    tlg_manager->message = tlg_message;
    tlg_manager->callback_query = tlg_callback_query;
    if (tlg_manager->begin()) {
      xTaskCreatePinnedToCore(getTLGUpdate, "Telegram update task", STACK_SIZE, NULL, tskIDLE_PRIORITY, &getTLGUpdateTask, tskNO_AFFINITY);
    } else {
      sendAlert("Ошибка авторизации на сервере Telegram, или проблемы с сетью. Попробуйте перезагрузить.");
      tlg_manager->stop();
      delete (tlg_manager); tlg_manager = nullptr;
    }
  } else {
    if (tlg_manager) tlg_manager->stop();
  }
}

void enable_mqtt(bool value) {
  if (value) {
    if (mqtt_manager) return;
    mqtt_manager = new MQTTManager(
      settings_manager->settings.mqtt_server,
      settings_manager->settings.mqtt_port,
      settings_manager->settings.mqtt_login,
      settings_manager->settings.mqtt_passwd
    );
    mqtt_manager->device_info = device_info;
    mqtt_manager->setClientID(device_info->mqtt_entity_id);
    mqtt_manager->getMQTTClient()->setCallback(mqtt_callback);
    entity_configuration(mqtt_manager->getMQTTClient());
    mqtt_manager->begin();
  } else {
    if (mqtt_manager) {
      delete (mqtt_manager); mqtt_manager = nullptr;
      entity_delete();
    }
  }
}

void save_settings(){
  settings_manager->SaveSettings(aFS);
  wifi_manager->setSSID(settings_manager->settings.wifi_ssid);
  wifi_manager->setPasswd(settings_manager->settings.wifi_passwd);
  enable_mqtt(false);
  enable_tlg(false);
  delay(1000);
  enable_mqtt(settings_manager->settings.server_type == 1);
  enable_tlg(settings_manager->settings.server_type == 2);
}

void factory_reset() {
  ESP_LOGW(TAG, "%s", "Factory reset");
  gpio_set_level(led_status, 1);
  delay(50);
  gpio_set_level(led_status, 0);
  delay(50);
  gpio_set_level(led_status, 1);
  delay(50);
  gpio_set_level(led_status, 0);
  delay(50);
  gpio_set_level(led_status, 1);
  delay(50);
  gpio_set_level(led_status, 0);
  delay(50);
  gpio_set_level(led_status, 1);
  settings_manager->ResetSettings();
  save_settings();
}


void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    std::string json_text = (char*)data;
    ESP_LOGI (TAG, "%s", json_text.c_str());
    JsonDocument doc;
    deserializeJson(doc, json_text);
    if (doc["method"] == "getSettings") { ws.textAll(settings_manager->getSettings().c_str());
                                          ws.textAll(getStatus().c_str());
                                          ws.textAll(getMediaExists().c_str());
                                          return; }
    if (doc["method"] == "setMode")     { setMode(doc["value"].as<uint8_t>()); return; }
    if (doc["method"] == "setAccept")   { setAccept(doc["value"].as<bool>()); return; }
    if (doc["method"] == "setDelivery") { setDelivery(doc["value"].as<bool>()); return; }
    if (doc["method"] == "setReject")   { setReject(doc["value"].as<bool>()); return; }
    if (doc["method"] == "setDelayBeforeAnswer") { ws.textAll(settings_manager->setDelayBeforeAnswer(doc["value"].as<uint16_t>()).c_str()); return; }
    if (doc["method"] == "setDelayOpen") { ws.textAll(settings_manager->setDelayOpen(doc["value"].as<uint16_t>()).c_str()); return; }
    if (doc["method"] == "setDelayAfterClose") { ws.textAll(settings_manager->setDelayAfterClose(doc["value"].as<uint16_t>()).c_str()); return; }
    if (doc["method"] == "setDelayFilter") { ws.textAll(settings_manager->setDelayFilter(doc["value"].as<uint16_t>()).c_str()); return; }
    if (doc["method"] == "setCallEndDelay") { ws.textAll(settings_manager->setCallEndDelay(doc["value"].as<uint16_t>()).c_str()); return; }
    if (doc["method"] == "setGreetingDelay") { ws.textAll(settings_manager->setGreetingDelay(doc["value"].as<uint16_t>()).c_str()); return; }
    if (doc["method"] == "setLed")      { setLed(doc["value"].as<bool>()); return; }
    if (doc["method"] == "setSound")    { setSound(doc["value"].as<bool>()); return; }
    if (doc["method"] == "setGreeting") { ws.textAll(settings_manager->setGreeting(doc["value"].as<bool>()).c_str()); return; }
    if (doc["method"] == "setMute")     { setMute(doc["value"].as<bool>()); return; }
    if (doc["method"] == "setRetain")   { ws.textAll(settings_manager->setRetain(doc["value"].as<bool>()).c_str()); return; }
    if (doc["method"] == "setChildLock"){ ws.textAll(settings_manager->setChildLock(doc["value"].as<bool>()).c_str()); return; }
    if (doc["method"] == "setPhoneDisable") { setPhoneDisable(doc["value"].as<bool>()); return; }
    if (doc["method"] == "setSSID") { ws.textAll(settings_manager->setSSID(doc["value"].as<std::string>()).c_str()); return; }
    if (doc["method"] == "setWIFIPassword") { ws.textAll(settings_manager->setWIFIPassword(doc["value"].as<std::string>()).c_str()); return; }
    if (doc["method"] == "setServerType") {
      ws.textAll(settings_manager->setServerType(doc["value"].as<uint8_t>()).c_str());
      enable_mqtt(settings_manager->settings.server_type == 1);
      enable_tlg(settings_manager->settings.server_type == 2);    
      return;
    }
    if (doc["method"] == "setMQTTServer") { ws.textAll(settings_manager->setMQTTServer(doc["value"].as<std::string>()).c_str()); return; }
    if (doc["method"] == "setMQTTPort") { ws.textAll(settings_manager->setMQTTPort(doc["value"].as<uint16_t>()).c_str()); return; }
    if (doc["method"] == "setMQTTLogin") { ws.textAll(settings_manager->setMQTTLogin(doc["value"].as<std::string>()).c_str()); return; }
    if (doc["method"] == "setMQTTPassword") { ws.textAll(settings_manager->setMQTTPassword(doc["value"].as<std::string>()).c_str()); return; }
    if (doc["method"] == "setTLGToken") { ws.textAll(settings_manager->setTLGToken(doc["value"].as<std::string>()).c_str()); return; }
    if (doc["method"] == "setTLGUser") { ws.textAll(settings_manager->setTLGUser(doc["value"].as<std::string>()).c_str()); return; }
    if (doc["method"] == "setCodeLifeTime") { ws.textAll(settings_manager->setCodeLifeTime(doc["value"].as<uint16_t>()).c_str()); return; }
    if (doc["method"] == "code") {
      std::string value = doc["value"].as<std::string>();
      if (value == "generate") tlg_code_generate();
      if (value == "delete") tlg_code_delete();
      return;
    }
    if (doc["method"] == "enableFTP") { ws.textAll(enable_ftp_server(doc["value"].as<bool>()).c_str()); return; }
    if (doc["method"] == "setUserLogin") { ws.textAll(settings_manager->setUserLogin(doc["value"].as<std::string>()).c_str()); return; }
    if (doc["method"] == "setUserPassword") { ws.textAll(settings_manager->setUserPassword(doc["value"].as<std::string>()).c_str()); return; }
    if (doc["method"] == "setAuth") { 
      ws.textAll(settings_manager->setAuth(doc["value"].as<bool>()).c_str());
      settings_manager->SaveSettings(aFS);
      sendAlert("Перезагрузите устройство.");
      return; 
    }
    if (doc["method"] == "save_params") { save_settings(); sendAlert("Настройки сохранены"); return; }
    if (doc["method"] == "restart") { 
      sendAlert("Устройство перезагружается");
      ESP.restart(); return;
    }
    if (doc["method"] == "mediaDelete") {
      std::string value = doc["value"].as<std::string>();
      std::string file = "";
      if (value == "access_allowed") file = ACCEPT_FILENAME;
      if (value == "greeting_allowed") file = GREETING_FILENAME;
      if (value == "delivery_allowed") file = DELIVERY_FILENAME;
      if (value == "access_denied") file = REJECT_FILENAME;
      if (file != "" && aFS.exists(file.c_str()) && aFS.remove(file.c_str())) ws.textAll(getMediaExists().c_str());
      else sendAlert("Ошибка удаления файла "+ value);
    }
  }
}

void onRequest(AsyncWebServerRequest *request){
  if (!wifi_manager->Connected()) {
    if (hw_status.web_services_init) request->redirect("/");
  } else request->send(404);
}
void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){}
void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){ 
  if (!index) {
    request->_tempFile = aFS.open("/media/" + filename, "w");
    ESP_LOGI(TAG, "Start upload file %s", filename.c_str());
  }
  if (len) request->_tempFile.write(data, len);
  if (final) {
    ESP_LOGI(TAG, "File upload complete %s", filename.c_str());
    ws.textAll(getMediaExists().c_str());
    request->_tempFile.close();
  }
}

void onREST(AsyncWebServerRequest *request) {
  JsonDocument json;
  uint8_t params = request->params();
  for(uint8_t i=0;i<params;i++){
    AsyncWebParameter* p = request->getParam(i);
    if(p->isFile()){
      request->send(404);
    } else {
      if (p->name() == "ftp") {
        if (p->value() != "") ws.textAll(enable_ftp_server(p->value() == "true").c_str());
        json["ftp"] = settings_manager->settings.ftp;
        continue;
      }
      if (p->name() == "send") {
        if (p->value() != "")
        json["send"] = tlg_manager?(tlg_manager->sendMessage(settings_manager->settings.tlg_user, p->value().c_str(), true)?"ok":"error"):"telegram not active";
        continue;
      }
      if (p->name() == "mute") {
        if (p->value() != "") setMute(p->value() == "true");
        json["mute"] = settings_manager->settings.mute;
        continue;
      }
      if (p->name() == "heap") {
        json["heap"] = ESP.getFreeHeap();
        continue;
      }
      if (p->name() == "reset") {
        factory_reset();
        json["reset"] = "ok";
        continue;
      }
      if (p->name() == "accept") {
        if (p->value() != "") setAccept(p->value() == "true");
        json["accept_call"] = settings_manager->settings.accept_call;
        json["delivery"] = settings_manager->settings.delivery;
        json["reject_call"] = settings_manager->settings.reject_call;
        continue;
      }
      if (p->name() == "reject") {
        if (p->value() != "") setReject(p->value() == "true");
        json["accept_call"] = settings_manager->settings.accept_call;
        json["delivery"] = settings_manager->settings.delivery;
        json["reject_call"] = settings_manager->settings.reject_call;
        continue;
      }
      if (p->name() == "delivery") {
        if (p->value() != "") setDelivery(p->value() == "true");
        json["accept_call"] = settings_manager->settings.accept_call;
        json["delivery"] = settings_manager->settings.delivery;
        json["reject_call"] = settings_manager->settings.reject_call;
        continue;
      }
      if (p->name() == "mode") {
        if (p->value() != "") setMode(atoi(p->value().c_str()));
        json["modes"] = settings_manager->settings.modes;
        continue;
      }
      if (p->name() == "restart" || p->name() == "reboot") {
        json["restart"] = "ok";
        std::string message;
        serializeJson(json, message);
        request->send(200, "text/html", message.c_str());
        sendAlert("Устройство перезагружается");
        ESP.restart();
        continue;
      }
      json[p->name().c_str()] = "method undefined";
      continue;
    }
  }
  std::string message;
  serializeJson(json, message);
  request->send(200, "text/html", message.c_str());
}

void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  switch (type) {
    case WS_EVT_CONNECT: ESP_LOGI(TAG, "WebSocket client #%u connected from %s", client->id(), client->remoteIP().toString().c_str());  break;
    case WS_EVT_DISCONNECT: ESP_LOGI(TAG, "WebSocket client #%u disconnected", client->id()); break;
    case WS_EVT_DATA: handleWebSocketMessage(arg, data, len); break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR: break;
  }
}

bool shouldReboot = false;

void web_server_init() {
  enable_ftp_server(!aFS.exists(INDEX_FILENAME));
  server.on("/api", HTTP_ANY, onREST);
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200);
  }, onUpload); 
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
    shouldReboot = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot?"OK":"FAIL");
    response->addHeader("Connection", "close");
    request->send(response);
  },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index){
      ESP_LOGI(TAG,"Update Start: %s\n", filename.c_str());
      if (!Update.begin()) {
        std::string error = Update.errorString();
        sendAlert(error);
        ESP_LOGI(TAG,"%s", error.c_str());
      }
    }
    if(!Update.hasError()){
      if(Update.write(data, len) != len) {
        std::string error = Update.errorString();
        sendAlert(error);
        ESP_LOGI(TAG,"%s", error.c_str());
      }
    }
    if(final){
      if(Update.end(true)) {
        sendAlert("Обновление успешно завершено. Перезагрузите устройство.");
        ESP_LOGI(TAG,"Update Success: %uB\n", index+len);
      }
      else {
        std::string error = Update.errorString();
        sendAlert(error);
        ESP_LOGI(TAG,"%s", error.c_str());
      }
    }
  });
  server.on(SETTING_FILENAME, HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(501, "text/html", "Access denied!");
  });
  if (settings_manager->settings.web_auth) {
    server.serveStatic("/", aFS, "/")
          .setCacheControl("max-age=604800") // 1 неделя;
          .setDefaultFile("index.html")
          .setAuthentication(settings_manager->settings.user_login.c_str(), settings_manager->settings.user_passwd.c_str());
          
  } else {
    server.serveStatic("/", aFS, "/")
      .setCacheControl("max-age=604800") // 1 неделя;
      .setDefaultFile("index.html");
  }
  server.onNotFound(onRequest);
  server.onFileUpload(onUpload);
  server.onRequestBody(onBody);
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();
  hw_status.web_services_init = true;
}

TaskHandle_t timeConfigTask;
void time_configure( void * pvParameters ) {   
    ESP_LOGI(TAG, "Configurate time. Connect to: %s", settings_manager->settings.time_server.c_str());
    configTime(0, 0, settings_manager->settings.time_server.c_str());
    time_t now = time(nullptr);
    while (now < 24 * 3600) {
      vTaskDelay(pdMS_TO_TICKS(100));
      now = time(nullptr);
    }
    randomSeed(micros());
    ESP_LOGI(TAG, "Time configurate complete. Current timestamp: %d", now); 
    hw_status.time_configure = true; 
    vTaskDelete(timeConfigTask);
}

TaskHandle_t wifiTask;
void wifi_loop ( void * pvParameters ) {   
    while (wifi_manager) { 
      wifi_manager->handle();
      if (settings_manager->settings.server_type == 1 && mqtt_manager) mqtt_manager->handle();
      if (settings_manager->settings.server_type == 2 && tlg_manager) {
        if (  settings_manager->settings.access_code != "" &&
              settings_manager->settings.access_code_lifetime &&
              settings_manager->access_code_expires &&
              settings_manager->access_code_expires <= millis() ) {
                tlg_code_delete();
                setAccept(false);
              }
      }
      if (settings_manager->settings.ftp && ftp_server) ftp_server->handleFTP();
      vTaskDelay(pdMS_TO_TICKS(10));    
    }
    vTaskDelete(wifiTask);
}

void setup() {
  Serial.begin(115200);
  /* Hardware setup */

  gpio_reset_pin(led_status);
  gpio_set_direction(led_status, GPIO_MODE_OUTPUT);
  gpio_set_drive_capability(led_status, GPIO_DRIVE_CAP_0);
  gpio_set_pull_mode(led_status, GPIO_FLOATING);

  gpio_reset_pin(led_indicator);
  gpio_set_direction(led_indicator, GPIO_MODE_OUTPUT);
  gpio_set_drive_capability(led_indicator, GPIO_DRIVE_CAP_0);
  gpio_set_pull_mode(led_indicator, GPIO_FLOATING);
  ledcSetup(0, 500, 8);
  ledcAttachPin(led_indicator, 0);

  gpio_reset_pin(detect_line);
  gpio_set_direction(detect_line, GPIO_MODE_INPUT);
  gpio_set_drive_capability(detect_line, GPIO_DRIVE_CAP_0);
  gpio_set_pull_mode(detect_line, GPIO_PULLUP_ONLY);

  gpio_reset_pin(button_boot);
  gpio_set_direction(button_boot, GPIO_MODE_INPUT);
  gpio_set_drive_capability(button_boot, GPIO_DRIVE_CAP_0);
  gpio_set_pull_mode(button_boot, GPIO_FLOATING);

  gpio_reset_pin(relay_line);
  gpio_set_direction(relay_line, GPIO_MODE_OUTPUT);
  gpio_set_drive_capability(relay_line, GPIO_DRIVE_CAP_0);
  gpio_set_pull_mode(relay_line, GPIO_FLOATING);

  gpio_reset_pin(switch_open);
  gpio_set_direction(switch_open, GPIO_MODE_OUTPUT);
  gpio_set_drive_capability(switch_open, GPIO_DRIVE_CAP_0);
  gpio_set_pull_mode(switch_open, GPIO_FLOATING);

  gpio_reset_pin(switch_phone);
  gpio_set_direction(switch_phone, GPIO_MODE_OUTPUT);
  gpio_set_drive_capability(switch_phone, GPIO_DRIVE_CAP_0);
  gpio_set_pull_mode(switch_phone, GPIO_FLOATING);

  attachInterrupt(detect_line, call_detector_enable, FALLING);
  
  audioOut->SetOutputModeMono(true);
  timer0 = timerBegin(0, 80, true); // 12,5 ns * 80 = 1000ns = 1us
  timerAttachInterrupt(timer0, &TimerHandler0, false); //edge interrupts do not work, use false
  timerAlarmWrite(timer0, 50000, true);
  timerAlarmEnable(timer0);
  /* System startup */
  device_status.line_status = l_status_close;
  ESP_LOGI(TAG, CONFIG_CHIP_DEVICE_PRODUCT_NAME);
  ESP_LOGI(TAG, CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION);
  ESP_LOGI(TAG, "System setup");
  if (!aFS.begin(true)) {
    ESP_LOGE(TAG, "%s", "An Error has occurred while mounting file system.");
    hw_status.last_error = 6;
    return;
  }
  ESP_LOGI(TAG, "Settings init");
  settings_manager = new SettingsManager(SETTING_FILENAME);
  settings_manager->LoadSettings(aFS);
  if (currentAction == WAIT) gpio_set_level(relay_line, settings_manager->settings.phone_disable); 

  ESP_LOGI(TAG, "WiFi init");
  wifi_manager = new WiFiManager(settings_manager->settings.wifi_ssid, settings_manager->settings.wifi_passwd);
  if (!hw_status.web_services_init) web_server_init();
  ESP_LOGI(TAG, "%s", "Web services init complete");

  rtc_wdt_set_length_of_reset_signal(RTC_WDT_SYS_RESET_SIG, RTC_WDT_LENGTH_3_2us);
  rtc_wdt_set_stage(RTC_WDT_STAGE0, RTC_WDT_STAGE_ACTION_RESET_SYSTEM);
  rtc_wdt_set_time(RTC_WDT_STAGE0, 500);
  ESP_LOGI(TAG, "%s", "WDT configured");

  xTaskCreatePinnedToCore(wifi_loop, "WiFi infinity loops", STACK_SIZE, NULL, tskIDLE_PRIORITY,  &wifiTask, tskNO_AFFINITY);
  ESP_LOGI(TAG, "%s", "WiFi monitor started");

  device_info = new DevInfo;
  device_info->name = CONFIG_CHIP_DEVICE_PRODUCT_NAME;
  device_info->manufacturer = "SCratORS";
  device_info->product = CONFIG_CHIP_DEVICE_PRODUCT_NAME;
  device_info->firmware = CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION;
  device_info->control = "http://"+wifi_manager->ip;
  std::string txt = WiFi.macAddress().c_str();
  size_t index;
  while ((index = txt.find(":")) != std::string::npos) txt.replace(index, 1, "");
  ESP_LOGI(TAG, "MQTT ID: %s", txt.c_str());
  device_info->mqtt_entity_id = txt;
  device_info->dev_name = "smartintercom";
  ESP_LOGI(TAG, "System started.");
}

void loop() {
  rtc_wdt_feed();
  if (wifi_manager->Connected()) {
      if (!hw_status.time_configure && !timeConfigTask) {
        ws.textAll(getStatus().c_str());
        sendAlert("Соединение с Wi-Fi Установлено!\nАдрес устройства: " + wifi_manager->ip);
        xTaskCreatePinnedToCore(time_configure, "Get global time", STACK_SIZE, NULL, tskIDLE_PRIORITY, &timeConfigTask, tskNO_AFFINITY);
        enable_mqtt(settings_manager->settings.server_type == 1);
        enable_tlg(settings_manager->settings.server_type == 2);
      } 
  }

  if (currentAction != WAIT) doAction(millis()-detectMillis);

  if (!settings_manager->settings.child_lock) {
    bool btnState = !gpio_get_level(button_boot);
    if (btnState && !btnPressFlag && millis() - last_toggle > DEBOUNCE_DELAY) {
        btnPressFlag = true;
        last_toggle = millis();
        if (hw_status.last_error) hw_status.last_error = 0;
        else if (settings_manager && settings_manager->last_error) settings_manager->last_error = 0;
        else if (wifi_manager && wifi_manager->last_error) wifi_manager->last_error = 0;
        else setAccept(!settings_manager->settings.accept_call);
    }
    if (btnState && btnPressFlag && millis() - last_toggle > LONGPRESS_DELAY) {
        last_toggle = millis();
        factory_reset();
    }
    if (!btnState && btnPressFlag && millis() - last_toggle > DEBOUNCE_DELAY) {
        btnPressFlag = false;
        last_toggle = millis();
    }
  }
}

