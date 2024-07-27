/* TODO
  - Телеграмм клиент
*/

#define CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION "Bluestreak 2.0.0-Web Insider Preview 07.2024 Firmware"
#define COPYRIGHT "SCratORS © 2024"
#define DISCOVERY_DELAY 500
#define led_status    16        // Индикатор статуса API, GPIO2 - это встроенный синий светодиод на ESP12
#define led_indicator 13        // Дополнительный индикатор, который будет показывать режимы и прочее.
#define detect_line   12        // Пин детектора вызова
#define button_boot   0         // Кнопка управления платой и перевода в режим прошивки
#define relay_line    14        // Пин "Переключение линии, плата/трубка"
#define switch_open   17        // Пин "Открытие двери"
#define switch_phone  4         // Пин "Трубка положена/поднята"
#define ACCEPT_FILENAME "/media/access_allowed.mp3"
#define REJECT_FILENAME "/media/access_denied.mp3"
#define DELIVERY_FILENAME "/media/delivery_allowed.mp3"
#define SETTING_FILENAME "/settings.json"
#define INDEX_FILENAME "/index.html"
#define l_status_call "Вызов"
#define l_status_answer "Ответ"
#define l_status_open "Открытие двери"
#define l_status_reject "Сброс вызова"
#define l_status_close "Закрыто"
#define STACK_SIZE 32768
#define CRITICAL_FREE 300000
#define TIME_SERVER "pool.ntp.org"

#include <ESPAsyncWebserver.h>
#include <Update.h>
#include "FTPServer.h"
#include "soc/rtc_wdt.h"
#include <ArduinoJson.h>
#include "wifi_manager.h"
#include "settings_manager.h"
#include "mqtt_manager.h"
#include "tlg_manager.h"

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
std::string modes_name = "Режим работы";
std::string sound_name = "Аудиосообщения";
std::string led_name = "Светоиндикация";
std::string mute_name = "Беззвучный режим";
std::string phone_disable_name = "Отключить трубку";
std::string accept_call_name = "Открыть дверь";
std::string reject_call_name = "Сбросить вызов";
std::string delivery_call_name = "Открыть курьеру";


enum {WAIT, CALLING, CALL, SWUP, VOICE, SWOPEN, DROP, ENDING, RESET};
uint8_t currentAction = WAIT;
bool questSenserProtect = false;
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
void ICACHE_RAM_ATTR call_detector();

uint8_t ledIndicatorCounter = 0;
uint8_t ledStatusCounter = 0;
uint8_t ledErrorCounter = 0;

void send_tlg_actions_kb(){
  // Отправляем клавиатуру выбора действия
  std::string welcome = "Входящий вызов в домофон!\n";
  std::string message = "✅ " + accept_call_name + "\t" + "🚚 " + mode_name[1] + "\t" + "🚷 " + mode_name[2];
  std::string commands =  "accept_once, delivery_once, reject_once";
  tlg_manager->sendMenu(welcome, message, commands, false);
}

void send_tlg_mode_kb(bool edit) {
  // Отправляем клавиатуру выбора режима
  std::string welcome = "Выбор постоянного режима работы:\n";
  std::string message = mode_name[0] + "\n" + mode_name[1] + "\n" + mode_name[2];
  std::string commands =  "mode_0, mode_1, mode_2";
  tlg_manager->sendMenu(welcome, message, commands, edit);
}

void send_tlg_start_kb(bool edit) {
  // Отправляем клавиатуру стартового меню
  std::string welcome = "SmartIntercom - Домофон:\n";
  std::string message = modes_name+": "+mode_name[settings_manager->settings.modes]+"\n" +
  (settings_manager->settings.accept_call?"🟢":"⚫️") + " " + accept_call_name + "\t" + 
  (settings_manager->settings.delivery?"🟢":"⚫️") + " " + delivery_call_name + "\t" + 
  (settings_manager->settings.reject_call?"🟢":"⚫️") + " " + reject_call_name + "\n" + 
  (settings_manager->settings.mute?"🟢":"⚫️") + " " + mute_name + "\t" + 
  (settings_manager->settings.sound?"🟢":"⚫️") + " " + sound_name + "\n" + 
  (settings_manager->settings.led?"🟢":"⚫️") + " " + led_name + "\t" + 
  (settings_manager->settings.phone_disable?"🟢":"⚫️") + " " + phone_disable_name;
  std::string commands = "modes, accept, delivery, reject, mute, sound, led, phone_disable";
  tlg_manager->sendMenu(welcome, message, commands, edit);
}

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
  if (tlg_manager) send_tlg_actions_kb();
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
      digitalWrite(led_status, (ledErrorCounter%10 < 5)?1:0);
      analogWrite(led_indicator, (ledErrorCounter%10 < 5)?80:1);
    }
    if (ledErrorCounter>160) ledErrorCounter = 0;
  } else if (settings_manager && settings_manager->last_error) {
    if (ledErrorCounter++ < settings_manager->last_error * 10) {
      digitalWrite(led_status, (ledErrorCounter%10 < 5)?1:0);
      analogWrite(led_indicator, (ledErrorCounter%10 < 5)?80:0);
    }
    if (ledErrorCounter>160) ledErrorCounter = 0;
  } else if (wifi_manager && wifi_manager->last_error) {
    if (ledErrorCounter++ < wifi_manager->last_error * 10) {
      digitalWrite(led_status, (ledErrorCounter%10 < 5)?1:0);
      analogWrite(led_indicator, (ledErrorCounter%10 < 5)?80:0);
    }
    if (ledErrorCounter>160) ledErrorCounter = 0;
  } else if (mqtt_manager && mqtt_manager->last_error) {
    if (ledErrorCounter++ < mqtt_manager->last_error * 10) {
      digitalWrite(led_status, (ledErrorCounter%10 < 5)?1:0);
      analogWrite(led_indicator, (ledErrorCounter%10 < 5)?80:0);
    }
    if (ledErrorCounter>160) ledErrorCounter = 0;
  } else if (tlg_manager && tlg_manager->last_error) {
    if (ledErrorCounter++ < tlg_manager->last_error * 10) {
      digitalWrite(led_status, (ledErrorCounter%10 < 5)?1:0);
      analogWrite(led_indicator, (ledErrorCounter%10 < 5)?80:0);
    }
    if (ledErrorCounter>160) ledErrorCounter = 0;
  } else {
    if (settings_manager && settings_manager->settings.led) {

      if (settings_manager->settings.reject_call) {
          switch (ledStatusCounter++) {
              case 0: digitalWrite(led_status, 1); break;
              case 1: digitalWrite(led_status, 0); break;
              case 40: ledStatusCounter = 0; break;
              default: if (ledStatusCounter > 40) ledStatusCounter = 0;
          }
      } else digitalWrite(led_status, 0);

      if (currentAction != WAIT) {
        switch (ledIndicatorCounter++) {
            case 0: analogWrite(led_indicator, 240); break;
            case 1: analogWrite(led_indicator, 0); ledIndicatorCounter = 0;break;
            default: if (ledIndicatorCounter > 1) ledIndicatorCounter = 0; break;
        }
      } else if (settings_manager->settings.accept_call) {
        switch (ledIndicatorCounter++) {
            case 0: analogWrite(led_indicator, 240); break;
            case 2: analogWrite(led_indicator, 0); break;
            case 40: ledIndicatorCounter = 0; break;
            default: if (ledIndicatorCounter > 40) ledIndicatorCounter = 0; break;
        }
      } else if (settings_manager->settings.mute) {
        ledIndicatorCounter++;
        if (ledIndicatorCounter < 40) {
            analogWrite(led_indicator, ledIndicatorCounter * 6);
        } else if (ledIndicatorCounter < 80) {
            analogWrite(led_indicator, (80 - ledIndicatorCounter) * 6);
        } else ledIndicatorCounter = 0;
      } else analogWrite(led_indicator, 0);
    } else {
      digitalWrite(led_status, 0);
      analogWrite(led_indicator, 0);
    }
  }
  portEXIT_CRITICAL_ISR(&timerMux);
}

void call_detector() {
  if (currentAction == WAIT) {
    if (settings_manager->settings.mute) {
      digitalWrite(switch_phone, 1);  
      digitalWrite(relay_line, 1);
    }
    detectMillis = millis();
    questSenserProtect = false;
    currentAction = CALLING;
  }
  if (currentAction == CALLING) detectMillis = millis();
}

void phone_disable_action () {
    if (currentAction == WAIT) {
      digitalWrite(switch_phone, settings_manager->settings.phone_disable); 
      digitalWrite(relay_line, settings_manager->settings.phone_disable);
    }
}

void doAction(uint32_t timer) {
  switch (currentAction) {
    case WAIT:  break;
    case CALLING: if (timer > settings_manager->settings.call_end_delay) currentAction = RESET;
                  if (timer > settings_manager->settings.delay_filter && digitalRead(detect_line)) {
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
                        }    
                  }
                  break;
    case CALL:  if (timer > settings_manager->settings.delay_before) {
                  digitalWrite(switch_phone, 0);  
                  digitalWrite(relay_line, 1);
                  setLineStatus(l_status_answer);
                  currentAction = SWUP;
                } break;
    case SWUP:  if (timer > settings_manager->settings.delay_before + 
                            settings_manager->settings.delay_before) {
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
                                    (settings_manager->settings.modes == 2 && !settings_manager->settings.reject_call)) ? SWOPEN : DROP;
                } break;
    case VOICE: if (!audioPlayer->loop()) {
                  audioPlayer->stop();
                  audioLength = millis() - audioLength;
                  delete audioPlayer;
                  delete audioFile;
                  currentAction = ( settings_manager->settings.delivery || 
                                    settings_manager->settings.accept_call || 
                                    (settings_manager->settings.modes == 2 && !settings_manager->settings.reject_call)) ? SWOPEN : DROP;
                } break;
    case SWOPEN:digitalWrite(switch_open, 1);
                setLineStatus(l_status_open); 
                currentAction = DROP;
                break;
    case DROP:  if (timer > settings_manager->settings.delay_before + 
                            settings_manager->settings.delay_before + 
                            audioLength + 
                            settings_manager->settings.delay_open) {
                  digitalWrite(switch_open, 0);
                  digitalWrite(switch_phone, 1); 
                  setLineStatus(l_status_reject);
                  currentAction = ENDING;
                } break;
    case ENDING:if (timer > settings_manager->settings.delay_before + 
                            settings_manager->settings.delay_before + 
                            audioLength + 
                            settings_manager->settings.delay_open + 
                            settings_manager->settings.delay_after) {
                  currentAction = RESET;
                } break;
    case RESET: digitalWrite(relay_line, settings_manager->settings.phone_disable);
                digitalWrite(switch_phone, settings_manager->settings.phone_disable); 
                digitalWrite(switch_open, 0);
                settings_manager->settings.accept_call = false;
                settings_manager->settings.delivery = false;
                settings_manager->settings.reject_call = false;
                device_status.line_detect = false;
                device_status.line_status = l_status_close;
                sendStatus();
                currentAction = WAIT;
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
      delete ftp_server;
      ftp_server = nullptr;
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

void tlg_callback(FB_msg& msg) {
  std::string cmd = msg.data.c_str();
  std::string txt = msg.text.c_str();
  ESP_LOGI(TAG,"Callback: cmd:%s txt:%s", cmd.c_str(), txt.c_str());
  if (txt == "/start") send_tlg_start_kb(false);
  else if (cmd == "") return;
  else if (cmd == "modes") send_tlg_mode_kb(true);
  else if (cmd == "accept_once") {setAccept(true); tlg_manager->sendMessage("Открываю дверь.");}
  else if (cmd == "delivery_once") {setDelivery(true); tlg_manager->sendMessage("Открываю дверь курьеру.");}
  else if (cmd == "reject_once") {setReject(true); tlg_manager->sendMessage("Сбрасываю вызов.");}
  else {
    if (cmd == "mode_0") setMode(0);
    if (cmd == "mode_1") setMode(1);
    if (cmd == "mode_2") setMode(2);
    if (cmd == "accept") setAccept(!settings_manager->settings.accept_call);
    if (cmd == "delivery") setDelivery(!settings_manager->settings.delivery);
    if (cmd == "reject") setReject(!settings_manager->settings.reject_call);
    if (cmd == "mute") setMute(!settings_manager->settings.mute);
    if (cmd == "sound") setMute(!settings_manager->settings.sound);
    if (cmd == "led") setLed(!settings_manager->settings.led);
    if (cmd == "phone_disable") setPhoneDisable(!settings_manager->settings.phone_disable);
    send_tlg_start_kb(true);
  }
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
  accept_once = new Switch("accept_call", mqtt_client, &settings_manager->settings.accept_call);
  reject_once = new Switch("reject_call", mqtt_client, &settings_manager->settings.reject_call);
  delivery_once = new Switch("delivery_call", mqtt_client, &settings_manager->settings.delivery);
  sound = new Switch("sound", mqtt_client, &settings_manager->settings.sound);
  led = new Switch("led", mqtt_client, &settings_manager->settings.led);
  mute = new Switch("mute", mqtt_client, &settings_manager->settings.mute);
  phone_disable = new Switch("phone_disable", mqtt_client, &settings_manager->settings.phone_disable);
  line_detect = new BinarySensor("line_detect", mqtt_client, &device_status.line_detect);
  line_status = new Sensor("line_status", mqtt_client, &device_status.line_status);
  modes = new Select("modes", mqtt_client, &settings_manager->settings.modes);

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

void enable_tlg(bool value){
  if (value) {
    if (tlg_manager) return;
    tlg_manager = new TLGManager(settings_manager->settings.tlg_token);
    tlg_manager->setUser(settings_manager->settings.tlg_user);
    tlg_manager->getTLGClient()->attach(tlg_callback);
    tlg_manager->begin();
  } else {
    if (tlg_manager) {
      delete (tlg_manager);
      tlg_manager = nullptr;
    }
  }
}

void enable_mqtt(bool value) {
  if (value) {
    if (mqtt_manager) return;
    mqtt_manager = new MQTTManager(settings_manager->settings.mqtt_server,
                                                      settings_manager->settings.mqtt_port,
                                                      settings_manager->settings.mqtt_login,
                                                      settings_manager->settings.mqtt_passwd);
    mqtt_manager->device_info = device_info;
    mqtt_manager->getMQTTClient()->setCallback(mqtt_callback);
    entity_configuration(mqtt_manager->getMQTTClient());
    mqtt_manager->begin();
  } else {
    if (mqtt_manager) {
      delete (mqtt_manager);
      mqtt_manager = nullptr;
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
  enable_mqtt(settings_manager->settings.server_type == 1);
  enable_tlg(settings_manager->settings.server_type == 2);
}

void factory_reset() {
  ESP_LOGW(TAG, "%s", "Factory reset");
  digitalWrite(led_status, 1);
  delay(50);
  digitalWrite(led_status, 0);
  delay(50);
  digitalWrite(led_status, 1);
  delay(50);
  digitalWrite(led_status, 0);
  delay(50);
  digitalWrite(led_status, 1);
  delay(50);
  digitalWrite(led_status, 0);
  delay(50);
  digitalWrite(led_status, 1);
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
    if (doc["method"] == "setLed")      { setLed(doc["value"].as<bool>()); return; }
    if (doc["method"] == "setSound")    { setSound(doc["value"].as<bool>()); return; }
    if (doc["method"] == "setMute")     { setMute(doc["value"].as<bool>()); return; }
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
    if (doc["method"] == "enableFTP") { ws.textAll(enable_ftp_server(doc["value"].as<bool>()).c_str()); return; }
    if (doc["method"] == "save_params") { save_settings(); sendAlert("Настройки сохранены"); return; }
    if (doc["method"] == "restart") { 
      sendAlert("Устройство перезагружается");
      ESP.restart(); return;
    }
    if (doc["method"] == "mediaDelete") {
      std::string value = doc["value"].as<std::string>();
      std::string file = "";
      if (value == "access_allowed") file = ACCEPT_FILENAME;
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

void onREST(AsyncWebServerRequest *request) {}

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
  server.on("/api", HTTP_POST, onREST);
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

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    factory_reset();
    request->send(200, "text/html", "Factory reset complete. Restart.");
    ESP.restart();
  });
  server.on(SETTING_FILENAME, HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(501, "text/html", "Access denied!");
  });
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){request->send(aFS, "/index.html", "text/html");});
  server.serveStatic("/", aFS, "/");
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
    ESP_LOGI(TAG, "Configurate time. Connect to: %s", TIME_SERVER);
    configTime(0, 0, TIME_SERVER);
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
      if (settings_manager->settings.server_type == 2 && tlg_manager) tlg_manager->handle();
      if (settings_manager->settings.ftp && ftp_server) ftp_server->handleFTP();
      vTaskDelay(pdMS_TO_TICKS(10));    
    }
    vTaskDelete(wifiTask);
}

void setup() {
  Serial.begin(115200);
  /* Hardware setup */
  pinMode(led_status, OUTPUT);
  pinMode(led_indicator, OUTPUT);
  pinMode(detect_line, INPUT_PULLUP);
  pinMode(button_boot, INPUT);
  pinMode(relay_line, OUTPUT);
  pinMode(switch_open, OUTPUT);
  pinMode(switch_phone, OUTPUT); 
  attachInterrupt(detect_line, call_detector, FALLING);
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
  if (currentAction == WAIT) digitalWrite(relay_line, settings_manager->settings.phone_disable); 

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

  bool btnState = !digitalRead(button_boot);
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

