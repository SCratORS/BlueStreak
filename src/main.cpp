#include <ESPAsyncWebserver.h>
#include <esp_task_wdt.h>
#include <Update.h>
#include <Syslog.h>
#include "FTPServer.h"
#include "driver/pcnt.h"
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
#include "button.h"
#include "binary_sensor.h"

#include "AudioGeneratorMP3.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
#include "LittleFS.h"
#include "AudioFileSourceLittleFS.h"
#define aFS LittleFS

#define WDT_TIMEOUT 4
esp_err_t ESP32_ERROR;

static const char* TAG = "MAIN";
std::string mode_name[3] = {"Не активен","Сброс вызова","Открывать всегда"}; 
enum {WAIT, CALLING, RING, CALL, SWUP, VOICE, PREOPEN, SWOPEN, SWCLOSE, GREETING, GREETING_VOICE, DROP, ENDING, RESET};
static uint8_t currentAction = WAIT;
static uint32_t detectMillis = 0;
static uint32_t audioLength = 0;
static uint64_t tlg_restart_timer;
static uint64_t reboot_timeout;
static bool webSocketLog = false;

static AudioOutputI2S *audioOut = new AudioOutputI2S(0, AudioOutputI2S::INTERNAL_DAC);
static AudioGenerator *audioPlayer;
static AudioFileSource *audioFile;
static JsonDocument json;
static std::string message;
static const uint16_t DEBOUNCE_DELAY = 100;
static const uint16_t LONGPRESS_DELAY = 5000;
static bool btnPressFlag = false;
static uint32_t last_toggle;

static struct {
  bool line_detect;
  std::string line_status;
} device_status;

static struct {
  bool web_services_init = false;
  bool time_configure = false;
  uint8_t last_error = 0;
} hw_status;

FTPServer * ftp_server;
MQTTManager * mqtt_manager;
TLGManager * tlg_manager;
WiFiManager * wifi_manager;
SettingsManager * settings_manager;
static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static HTTPClient https_client;
static HTTPClient https_client_post;
static WiFiUDP udpClient;

Syslog * syslog;

Switch * accept_once;
Switch * reject_once;
Switch * delivery_once;
Button * open_door;
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

static uint8_t ledIndicatorCounter = 0;
static uint8_t ledStatusCounter = 0;
static uint8_t ledErrorCounter = 0;

static int16_t currentAddressCounter = 0;
static bool syncCounter = false;
static bool triggerCounter = false;
static bool finishCounter = false;

std::string ReplaceString(std::string subject, const std::string& search,
                          const std::string& replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
    return subject;
}

void LOG(const char * format, ...) {
    char loc_buf[64];
    char * temp = loc_buf;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
    va_end(copy);
    if(len < 0) {
        va_end(arg);
        return;
    }
    if(len >= (int)sizeof(loc_buf)){  // comparation of same sign type for the compiler
        temp = (char*) malloc(len+1);
        if(temp == NULL) {
            va_end(arg);
            return;
        }
        len = vsnprintf(temp, len+1, format, arg);
    }
    va_end(arg);
    if (syslog) syslog->log(temp);
    
    if (webSocketLog) {
      std::string logtext = "{\"text\":\"" + ReplaceString(ReplaceString(std::string(temp), "\n", ""), "\"", "\\\"") + "\\n\"}";
      ws.textAll(logtext.c_str());
    }

    Serial.printf("[%d] ", millis());
    Serial.print(temp);
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
  message = "{\"line_status\":\"" + device_status.line_status + "\"}";
  LOG("[%s] %s\n", TAG, message.c_str());
  if (line_status) line_status->publishValue();
  ws.textAll(message.c_str());
}

void setLineDetect(bool value){
  device_status.line_detect = value;
  message = "{\"line_detect\":" + std::string(device_status.line_detect?"true":"false") + "}";
  LOG("[%s] %s\n", TAG, message.c_str());
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

void sendAlert(std::string value) {
  message = "{\"alert\":\"" + value + "\"}";
  LOG("[%s] %s\n", TAG, message.c_str());
  ws.textAll(message.c_str());
}

void sendStatus(){
  json.clear();
  message = "{\"accept_call\":" + std::string(settings_manager->settings.accept_call?"true":"false") + "," +
            "\"delivery\":" + std::string(settings_manager->settings.delivery?"true":"false") + "," +
            "\"reject_call\":" + std::string(settings_manager->settings.reject_call?"true":"false") + "," +
            "\"line_detect\":" + std::string(device_status.line_detect?"true":"false") + "," +
            "\"line_status\":\"" + device_status.line_status + "\"}";
  LOG("[%s] %s\n", TAG, message.c_str());
  ws.textAll(message.c_str());
  if (line_detect) line_detect->publishValue();
  if (line_status) line_status->publishValue();
  mqtt_publish_once_actions();
}

std::string getMediaExists() {
  std::string aap = std::string(ACCEPT_FILENAME) + ".mp3";
  std::string aapw = std::string(ACCEPT_FILENAME) + ".wav";
  std::string gap = std::string(GREETING_FILENAME) + ".mp3";
  std::string gapw = std::string(GREETING_FILENAME) + ".wav";
  std::string dap = std::string(DELIVERY_FILENAME) + ".mp3";
  std::string dapw = std::string(DELIVERY_FILENAME) + ".wav";
  std::string adp = std::string(REJECT_FILENAME) + ".mp3";
  std::string adpw = std::string(REJECT_FILENAME) + ".wav";
  std::string rgt = std::string(RINGTONE_FILENAME) + ".mp3";
  std::string rgtw = std::string(RINGTONE_FILENAME) + ".wav";
  message = "{\"fs_used\":\"" + get_fs_used() + "\"," +
            "\"access_allowed_play\":" + std::string(aFS.exists(aap.c_str()) ? std::string("\"" + aap + "\"") : (aFS.exists(aapw.c_str()) ? std::string("\"" + aapw + "\"")  : "null")) + "," +
            "\"greeting_allowed_play\":" + std::string(aFS.exists(gap.c_str()) ? std::string("\"" + gap + "\"") : (aFS.exists(gapw.c_str()) ? std::string("\"" + gapw + "\"")  : "null")) + "," +
            "\"ringtone_play\":" + std::string(aFS.exists(rgt.c_str()) ? std::string("\"" + rgt + "\"") : (aFS.exists(rgtw.c_str()) ? std::string("\"" + rgtw + "\"")  : "null")) + "," +
            "\"delivery_allowed_play\":" + std::string(aFS.exists(dap.c_str()) ? std::string("\"" + dap + "\"") : (aFS.exists(dapw.c_str()) ? std::string("\"" + dapw + "\"")  : "null")) + "," +
            "\"access_denied_play\":" + std::string(aFS.exists(adp.c_str()) ? std::string("\"" + adp + "\"") : (aFS.exists(adpw.c_str()) ? std::string("\"" + adpw + "\"")  : "null")) + "}";
  LOG("[%s] %s\n", TAG, message.c_str());
  return message;
}

std::string getStatus(){ 
  device_info->control = "http://"+wifi_manager->ip;
  message = "{\"ftp\":" + std::string(ftp_server?"true":"false") + "," +
            "\"ip\":\"" + wifi_manager->ip + "\"," +
            "\"line_detect\":" + std::string(device_status.line_detect?"true":"false") + "," +
            "\"line_status\":\"" + device_status.line_status + "\"," +
            "\"firmware\":\"" + std::string(CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION) + "\"," +
            "\"copyright\":\"" + std::string(COPYRIGHT) + "\"}";
  LOG("[%s] %s\n", TAG, message.c_str());
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
uint8_t max(uint8_t a, uint8_t b) { return a>b?a:b; }
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
void IRAM_ATTR TimerHandler0() {
  portENTER_CRITICAL_ISR(&timerMux);
  uint8_t error = max(hw_status.last_error,
                  max(settings_manager?settings_manager->last_error:0,
                  max(wifi_manager?wifi_manager->last_error:0,
                  max(mqtt_manager?mqtt_manager->last_error:0, tlg_manager?tlg_manager->last_error:0)))) << 4;
  if (error) {
    if (ledErrorCounter++ < error) {
        gpio_set_level(led_status, (ledErrorCounter%16 < 8)?1:0);
        ledcWrite(0, (ledErrorCounter%16 < 8)?60:0);
    }
    if (ledErrorCounter>160) ledErrorCounter = 0;
  } else {
    if (settings_manager->settings.led) {
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

void deleteAudio(){
  audioPlayer->stop();
  audioLength = millis() - audioLength;
  delete audioPlayer; audioPlayer = nullptr;
  delete audioFile; audioFile = nullptr; 
}


uint64_t reset_time = 0;
bool ringplay = false;
void calling_detect() {
  if (currentAction == WAIT) {
    if (settings_manager->settings.mute && 
        (settings_manager->settings.accept_call ||
        settings_manager->settings.reject_call ||
        settings_manager->settings.modes )) {
      gpio_set_level(switch_phone, 1);  
      gpio_set_level(relay_line, 1);
    } 
    ringplay = false;
    detectMillis = millis();
    currentAction = CALLING;
  }
}


uint64_t last_time_detect = 0;
void IRAM_ATTR call_detector_isr() {
  if (gpio_get_level(detect_line)) {
    last_time_detect = millis();
    return;
  }
  /*
  reset_time = millis();
  if (settings_manager->settings.address_counter) {
    if (reset_time - last_time_detect > 200) syncCounter = !syncCounter;
    if (currentAction == WAIT) {
      if (syncCounter) {
        finishCounter = false;
        currentAddressCounter = 0;
        pcnt_counter_clear(PCNT_UNIT_0);
      }
    }
    if (currentAction == CALLING) {
      if (!syncCounter && finishCounter && !triggerCounter) currentAction = RESET;
    }
  } else {
    if (currentAction == WAIT) calling_detect();
  }
*/
  reset_time = millis();
    if (currentAction == WAIT) {
    if (settings_manager->settings.address_counter) {
      if (!syncCounter) {
        syncCounter = true;
        currentAddressCounter = 0;
        pcnt_counter_clear(PCNT_UNIT_0);
        finishCounter = false;
      }
    } else calling_detect();
  }
  //Если триггер сработал (т.е. мы с цифрой имеем дело, а значит синхра уже была и счетчик закончил счет)
  //И мы находимся в режиме детекции вызова, но отработка не запущена, и поймали сигнал отбоя, то
  //сравниваем время импульса с длиной синхро импульса, и сбрасываем детектор если это оно.
  if (triggerCounter && currentAction == CALLING) {
      if (reset_time - last_time_detect > settings_manager->settings.counter_duration) {
        triggerCounter = false;
        currentAction = RESET;
      }
  }
}

void phone_disable_action () {
    if (currentAction == WAIT) {
      gpio_set_level(switch_phone, settings_manager->settings.phone_disable); 
      gpio_set_level(relay_line, !settings_manager->settings.address_counter && settings_manager->settings.phone_disable);
    }
}

bool initAudio(const char * filename) {
  if (settings_manager->settings.server_type < 2) {
    if (aFS.exists(String(filename) + ".mp3")) {
      audioFile = new AudioFileSourceLittleFS((String(filename) + ".mp3").c_str());
      if (audioFile) {
          audioPlayer = new AudioGeneratorMP3();
          if (audioPlayer && audioPlayer->begin(audioFile, audioOut)) {
            audioLength = millis();
            return true;
          }
      }
    }
  }
  if (aFS.exists(String(filename) + ".wav")) {
    audioFile = new AudioFileSourceLittleFS((String(filename) + ".wav").c_str());
    if (audioFile) {
        audioPlayer = new AudioGeneratorWAV();
        if (audioPlayer && audioPlayer->begin(audioFile, audioOut)) {
          audioLength = millis();
          return true;
        }
    }
  }   
  LOG("[%s] %s\n", TAG, "Error open audio file");
  return false;
}

uint64_t timerAction = 0;
void doAction(uint32_t timer) {
  switch (currentAction) {
    case WAIT:  break;
    case CALLING: if (!triggerCounter && millis() - reset_time > settings_manager->settings.call_end_delay) currentAction = RESET;
                  if (timer > settings_manager->settings.delay_filter && !gpio_get_level(detect_line)) {
                    if (!device_status.line_detect) {
                      setLineDetect(true);
                      setLineStatus(l_status_call);
                    }
                    if (settings_manager->settings.accept_call ||
                        settings_manager->settings.reject_call ||
                        settings_manager->settings.modes ) {
                          currentAction = CALL;
                          detectMillis = millis(); 
                          timerAction += settings_manager->settings.delay_system;      
                        } else {
                          if (!ringplay && settings_manager->settings.ringtone) {
                            ringplay = true;
                            if (audioPlayer) deleteAudio();
                            if (initAudio(RINGTONE_FILENAME)) {
                              gpio_set_level(switch_phone, 0);  
                              gpio_set_level(relay_line, 1);
                              currentAction = RING;
                              LOG("[%s] %s\n", TAG, "Play");
                            }
                          }
                        }   
                  } break;
    case RING:    if (!audioPlayer->loop()) {
                    deleteAudio();
                    gpio_set_level(relay_line, 0);
                    ringplay = true;
                    currentAction = CALLING;
                  } 
                  if (settings_manager->settings.accept_call ||
                    settings_manager->settings.reject_call ||
                    settings_manager->settings.modes ) {
                      currentAction = CALL;
                      detectMillis = millis(); 
                      timerAction += settings_manager->settings.delay_system;      
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
                    if (audioPlayer) deleteAudio();
                    if (initAudio(settings_manager->settings.delivery ? DELIVERY_FILENAME : 
                                  settings_manager->settings.accept_call ? ACCEPT_FILENAME :
                                  settings_manager->settings.reject_call ? REJECT_FILENAME :
                                  settings_manager->settings.modes == 2 ? ACCEPT_FILENAME : REJECT_FILENAME)) {
                      currentAction = VOICE;
                      break;
                    }
                  }
                  audioLength = 0;
                  currentAction = ( settings_manager->settings.accept_call || 
                                    (settings_manager->settings.modes == 2 && !settings_manager->settings.reject_call)) ? PREOPEN : DROP;
                  if (currentAction == PREOPEN) timerAction += settings_manager->settings.delay_system;
                } break;
    case VOICE: if (!audioPlayer->loop()) {
                  deleteAudio();
                  currentAction = ( settings_manager->settings.accept_call || 
                                    (settings_manager->settings.modes == 2 && !settings_manager->settings.reject_call)) ? SWOPEN : DROP;
                } break;
    case PREOPEN: if (timer > timerAction) currentAction = SWOPEN;
                  break;
    case SWOPEN:  if (settings_manager->settings.force_open) gpio_set_level(relay_line, 0);
                  else gpio_set_level(switch_open, 1);
                  setLineStatus(l_status_open);
                  timerAction += (audioLength + settings_manager->settings.delay_open);
                  currentAction = settings_manager->settings.greeting?GREETING:SWCLOSE;
                  break;
    case SWCLOSE: if (timer > timerAction) {
                    gpio_set_level(switch_open, 0);
                    timerAction += settings_manager->settings.delay_system;
                    currentAction = DROP;
                  } break;
    case GREETING: if (timer > timerAction) {
                    if (settings_manager->settings.sound) {
                      if (initAudio(GREETING_FILENAME)) {
                        setLineStatus(l_status_answer);
                        currentAction = GREETING_VOICE;
                        if (settings_manager->settings.force_open) gpio_set_level(relay_line, 1);
                        else gpio_set_level(switch_open, 0);
                        timerAction += settings_manager->settings.greeting_delay;
                        break;
                      }
                    }
                    currentAction = DROP;
                  } break;
    case GREETING_VOICE: if (timer > timerAction) {
                    if (!audioPlayer->loop()) {
                      deleteAudio();
                      timerAction += audioLength;
                      currentAction = DROP;
                    }
                  } break;
    case DROP:  if (timer > timerAction) {
                  if (settings_manager->settings.force_open) gpio_set_level(relay_line, 1);
                  else gpio_set_level(switch_open, 0);
                  gpio_set_level(switch_phone, 1); 
                  setLineStatus(l_status_reject);
                  currentAction = ENDING;
                  timerAction += settings_manager->settings.delay_after;
                } break;
    case ENDING:if (timer > timerAction) currentAction = RESET;
                break;
    case RESET: gpio_set_level(relay_line, !settings_manager->settings.address_counter && settings_manager->settings.phone_disable);
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
                if (audioPlayer) deleteAudio();
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
  message = "{\"ftp\":" + std::string(value?"true":"false") + "}";
  LOG("[%s] %s\n", TAG, message.c_str());
  return message;
}

void openAction(){
  if (currentAction == WAIT || currentAction == RING || currentAction == CALLING) {
    settings_manager->settings.accept_call = true;
    currentAction = CALL;
    detectMillis = millis(); 
    timerAction += settings_manager->settings.delay_system;      
  }  
}

void setMode(uint8_t value) {
  ws.textAll(settings_manager->setMode(value).c_str());
  if (modes) modes->publishValue();
}

void setAccept(bool value) {
  ws.textAll(settings_manager->setAccept(value).c_str());
  mqtt_publish_once_actions();
  if (value && currentAction == CALLING) {
    currentAction = CALL;
    detectMillis = millis(); 
    timerAction += settings_manager->settings.delay_system;  
  }
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
            tlg_manager->sendMessage(settings_manager->settings.tlg_user, "✉️ Cообщение от пользователя @" + user_name + " :\n" + message);
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
  LOG("[%s] MQTT TOPIC: %s MESSAGE: %s\n", TAG, strTopic.c_str(), message.c_str());
  mqtt_manager->getEntity(strTopic.c_str())->callback(message);
  if (strTopic == modes->callback_topic) setMode(settings_manager->settings.modes);
  else if (strTopic == led->callback_topic) setLed(settings_manager->settings.led); 
  else if (strTopic == sound->callback_topic) setSound(settings_manager->settings.sound); 
  else if (strTopic == phone_disable->callback_topic) setPhoneDisable(settings_manager->settings.phone_disable);
  else if (strTopic == mute->callback_topic) setMute(settings_manager->settings.mute);
  else if (strTopic == accept_once->callback_topic) setAccept(settings_manager->settings.accept_call);
  else if (strTopic == delivery_once->callback_topic) setDelivery(settings_manager->settings.delivery);
  else if (strTopic == reject_once->callback_topic) setReject(settings_manager->settings.reject_call);
  else if (strTopic == open_door->callback_topic) openAction();
}

void entity_configuration(PubSubClient * mqtt_client) {
  accept_once = new Switch("accept_call", mqtt_client, &settings_manager->settings.accept_call, &settings_manager->settings.mqtt_retain);
  reject_once = new Switch("reject_call", mqtt_client, &settings_manager->settings.reject_call, &settings_manager->settings.mqtt_retain);
  delivery_once = new Switch("delivery_call", mqtt_client, &settings_manager->settings.delivery, &settings_manager->settings.mqtt_retain);
  open_door = new Button("oper_door", mqtt_client, nullptr, &settings_manager->settings.mqtt_retain);

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
  open_door->ic = "mdi:lock-open";open_door->friendly_name = open_door_name; mqtt_manager->addEntity(open_door);
  line_detect->friendly_name = "Детектор вызова"; mqtt_manager->addEntity(line_detect);
  sound->ent_cat = "config";sound->ic = "mdi:microphone-message";sound->friendly_name = sound_name; mqtt_manager->addEntity(sound);
  led->ent_cat = "config";led->ic = "mdi:led-on";led->friendly_name = led_name; mqtt_manager->addEntity(led);
  mute->ent_cat = "config";mute->ic = "mdi:volume-off";mute->friendly_name = mute_name; mqtt_manager->addEntity(mute);
  phone_disable->ent_cat = "config";phone_disable->ic = "mdi:phone-off";phone_disable->friendly_name = phone_disable_name; mqtt_manager->addEntity(phone_disable);
}

void entity_delete() {
  delete(accept_once);accept_once = nullptr;
  delete(reject_once);reject_once = nullptr;
  delete(delivery_once);delivery_once = nullptr;
  delete(open_door);open_door = nullptr;
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
  while (tlg_manager && tlg_manager->enabled()) tlg_manager->getUpdate();
  while (tlg_manager && tlg_manager->await()) tlg_manager->stop();
  delete (tlg_manager); tlg_manager = nullptr;
  vTaskDelete(getTLGUpdateTask);
}

void enable_tlg(bool value){
  if (value) {
    if (settings_manager->settings.tlg_token == "") return;
    if (tlg_manager) return;
    tlg_manager = new TLGManager(&https_client, &https_client_post);
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

std::string enable_force_open(bool value) {
  settings_manager->settings.force_open = value;
  message = "{\"force_open\":" + std::string(settings_manager->settings.force_open?"true":"false") + "}";
  LOG("[%s] %s\n", TAG, message.c_str());
  return message;
}

std::string enable_syslog(bool value) {
  if (value) {
    if (wifi_manager)
      if (!syslog && settings_manager->settings.syslog_server != "") syslog = new Syslog(udpClient,
      settings_manager->settings.syslog_server.c_str(),
      settings_manager->settings.syslog_port,
      CONFIG_CHIP_DEVICE_PRODUCT_NAME,CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION,LOG_KERN);
  } else {
    if (syslog) {
      delete (syslog); syslog = nullptr;
    }
  }
  settings_manager->settings.syslog = syslog?true:false;
  message = "{\"syslog\":" + std::string(settings_manager->settings.syslog?"true":"false") + "}";
  LOG("[%s] %s\n", TAG, message.c_str());
  return message;
}

void save_settings(){
  settings_manager->SaveSettings(aFS);
  wifi_manager->setSSID(settings_manager->settings.wifi_ssid);
  wifi_manager->setPasswd(settings_manager->settings.wifi_passwd);
  enable_mqtt(false);
  enable_tlg(false);
  enable_syslog(false);
  delay(1000);
  enable_syslog(settings_manager->settings.syslog);
  enable_mqtt(settings_manager->settings.server_type == 1);
  enable_tlg(settings_manager->settings.server_type == 2);
}

void factory_reset() {
  LOG("[%s] Factory reset\n", TAG);
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
    LOG("[%s] %s\n", TAG, json_text.c_str());
    JsonDocument doc;
    deserializeJson(doc, json_text);
    if (doc["method"] == "enableLog") {webSocketLog = doc["value"].as<bool>(); return;}
    if (doc["method"] == "getSettings") { ws.textAll(settings_manager->getSettings().c_str());
                                          ws.textAll(getStatus().c_str());
                                          ws.textAll(getMediaExists().c_str());
                                          return; }
    if (doc["method"] == "open") { openAction(); return; }
    if (doc["method"] == "setMode")     { setMode(doc["value"].as<uint8_t>()); return; }
    if (doc["method"] == "setAccept")   { setAccept(doc["value"].as<bool>()); return; }
    if (doc["method"] == "setDelivery") { setDelivery(doc["value"].as<bool>()); return; }
    if (doc["method"] == "setReject")   { setReject(doc["value"].as<bool>()); return; }
    if (doc["method"] == "setDelaySystem") { ws.textAll(settings_manager->setDelaySystem(doc["value"].as<uint16_t>()).c_str()); return; }
    if (doc["method"] == "setDelayBeforeAnswer") { ws.textAll(settings_manager->setDelayBeforeAnswer(doc["value"].as<uint16_t>()).c_str()); return; }
    if (doc["method"] == "setDelayOpen") { ws.textAll(settings_manager->setDelayOpen(doc["value"].as<uint16_t>()).c_str()); return; }
    if (doc["method"] == "setDelayAfterClose") { ws.textAll(settings_manager->setDelayAfterClose(doc["value"].as<uint16_t>()).c_str()); return; }
    if (doc["method"] == "setDelayFilter") { ws.textAll(settings_manager->setDelayFilter(doc["value"].as<uint16_t>()).c_str()); return; }
    if (doc["method"] == "setCallEndDelay") { ws.textAll(settings_manager->setCallEndDelay(doc["value"].as<uint16_t>()).c_str()); return; }
    if (doc["method"] == "setGreetingDelay") { ws.textAll(settings_manager->setGreetingDelay(doc["value"].as<uint16_t>()).c_str()); return; }
    if (doc["method"] == "setRebootTimeout") { ws.textAll(settings_manager->setRebootTimeout(doc["value"].as<uint8_t>()).c_str()); return; }
    if (doc["method"] == "setCounterDuration") { ws.textAll(settings_manager->setCounterDuration(doc["value"].as<uint16_t>()).c_str()); return;}
    if (doc["method"] == "setImpulseFilter") { ws.textAll(settings_manager->setImpulseFilter(doc["value"].as<uint16_t>()).c_str()); return;}
    if (doc["method"] == "setLed")      { setLed(doc["value"].as<bool>()); return; }
    if (doc["method"] == "setRoom")     { ws.textAll(settings_manager->setAddressCounter(doc["value"].as<uint8_t>()).c_str()); return; }
    if (doc["method"] == "setSound")    { setSound(doc["value"].as<bool>()); return; }
    if (doc["method"] == "setGreeting") { ws.textAll(settings_manager->setGreeting(doc["value"].as<bool>()).c_str()); return; }
    if (doc["method"] == "setRingtone") { ws.textAll(settings_manager->setRingtone(doc["value"].as<bool>()).c_str()); return; }
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
      if (value == "ringtone_file") file = RINGTONE_FILENAME;
      if (file != "")
        if (aFS.exists((file + ".mp3").c_str()) && aFS.remove((file + ".mp3").c_str())) ws.textAll(getMediaExists().c_str());
        else if (aFS.exists((file + ".wav").c_str()) && aFS.remove((file + ".wav").c_str())) ws.textAll(getMediaExists().c_str());
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
    String f_name = filename.substring(0, filename.lastIndexOf("."));
    if (aFS.exists("/media/" + f_name + ".mp3")) aFS.remove("/media/" + f_name + ".mp3");
    if (aFS.exists("/media/" + f_name + ".wav")) aFS.remove("/media/" + f_name + ".wav");
    request->_tempFile = aFS.open("/media/" + filename, "w");
    LOG("[%s] Start upload file %s\n", TAG, filename.c_str());
  }
  if (len) request->_tempFile.write(data, len);
  if (final) {
    LOG("[%s] File upload complete %s\n", TAG, filename.c_str());
    ws.textAll(getMediaExists().c_str());
    request->_tempFile.close();
  }
}

void onREST(AsyncWebServerRequest *request) {
  json.clear();
  uint8_t params = request->params();
  for(uint8_t i=0;i<params;i++){
    const AsyncWebParameter* p = request->getParam(i);
    if(p->isFile()){
      request->send(404);
    } else {
      if (p->name() == "ftp") {
        if (p->value() != "") ws.textAll(enable_ftp_server(p->value() == "true").c_str());
        json["ftp"] = settings_manager->settings.ftp;
        continue;
      }
      if (p->name() == "reboot_timeout") {
        if (p->value() != "") ws.textAll(settings_manager->setRebootTimeout(atoi(p->value().c_str())).c_str());
        json["reboot_timeout"] = settings_manager->settings.reboot_timeout;
        continue;
      }

      if (p->name() == "counter_duration") {
        if (p->value() != "") ws.textAll(settings_manager->setCounterDuration(atoi(p->value().c_str())).c_str());
        json["counter_duration"] = settings_manager->settings.counter_duration;
        continue;
      }

      if (p->name() == "impulse_filter") {
        if (p->value() != "") ws.textAll(settings_manager->setImpulseFilter(atoi(p->value().c_str())).c_str());
        json["impulse_filter"] = settings_manager->settings.impulse_filter;
        continue;
      }

      if (p->name() == "force_open") {
        if (p->value() != "") ws.textAll(enable_force_open(p->value() == "true").c_str());
        json["force_open"] = settings_manager->settings.force_open;
        continue;
      }
      if (p->name() == "syslog") {
        if (p->value() != "") ws.textAll(enable_syslog(p->value() == "true").c_str());
        json["syslog"] = settings_manager->settings.syslog;
        continue;
      }
      if (p->name() == "syslog_port") {
        if (p->value() != "") ws.textAll(settings_manager->setSysLogPort(atoi(p->value().c_str())).c_str());
        json["syslog_port"] = settings_manager->settings.syslog_port;
        continue;
      }
      if (p->name() == "syslog_server") {
        if (p->value() != "") ws.textAll(settings_manager->setSysLogServer(p->value().c_str()).c_str());
        json["syslog_server"] = settings_manager->settings.syslog_server;
        continue;
      }
      if (p->name() == "send") {
        if (p->value() != "")
        json["send"] = tlg_manager->enabled()?(tlg_manager->sendMessage(settings_manager->settings.tlg_user, p->value().c_str(), true)?"ok":"error"):"telegram not active";
        continue;
      }
      if (p->name() == "mqtt_name") {
        if (p->value() != "") ws.textAll(settings_manager->setDevName(p->value().c_str()).c_str());
        json["mqtt_name"] = settings_manager->settings.dev_name;
        continue;
      }
      if (p->name() == "mute") {
        if (p->value() != "") setMute(p->value() == "true");
        json["mute"] = settings_manager->settings.mute;
        continue;
      }
      if (p->name() == "sys_delay") {
        if (p->value() != "") ws.textAll(settings_manager->setDelaySystem(atoi(p->value().c_str())).c_str());
        json["sys_delay"] = settings_manager->settings.delay_system;
        continue;
      }
      if (p->name() == "heap") {
        json["heap"] = ESP.getFreeHeap();
        json["max_buff"] = heap_caps_get_largest_free_block(0);
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
      if (p->name() == "uptime") {
        json["uptime"] = millis();
        continue;
      }
      if (p->name() == "restart" || p->name() == "reboot") {
        json["restart"] = "ok";
        serializeJson(json, message);
        request->send(200, "text/html", message.c_str());
        sendAlert("Устройство перезагружается");
        ESP.restart();
        continue;
      }
      if (p->name() == "room") {
        if (p->value() != "") ws.textAll(settings_manager->setAddressCounter(atoi(p->value().c_str())).c_str());
        json["room"] = settings_manager->settings.address_counter;
        continue;
      }
      if (p->name() == "save") {
        settings_manager->SaveSettings(aFS);
        json["save"] = "ok";
        continue;
      }
      json[p->name().c_str()] = "method undefined";
      continue;
    }
  }
  serializeJson(json, message);
  request->send(200, "text/html", message.c_str());
}

uint8_t ClientsCount = 0;
void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  switch (type) {
    case WS_EVT_CONNECT: LOG("[%s] WebSocket client #%u connected from %s\n", TAG, client->id(), client->remoteIP().toString().c_str()); ClientsCount++;  break;
    case WS_EVT_DISCONNECT: LOG("[%s] WebSocket client #%u disconnected\n", TAG, client->id()); if (ClientsCount) ClientsCount--; if (ClientsCount == 0) webSocketLog = false; break;
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
      LOG("[%s] Update Start: %s\n", TAG, filename.c_str());
      if (!Update.begin()) {
        std::string error = Update.errorString();
        sendAlert(error);
        LOG("[%s] %s\n", TAG, error.c_str());
      }
    }
    if(!Update.hasError()){
      if(Update.write(data, len) != len) {
        std::string error = Update.errorString();
        sendAlert(error);
        LOG("[%s] %s\n", TAG, error.c_str());
      }
    }
    if(final){
      if(Update.end(true)) {
        sendAlert("Обновление успешно завершено. Перезагрузите устройство.");
        LOG("[%s] Update Success: %uB\n", TAG, index+len);
      }
      else {
        std::string error = Update.errorString();
        sendAlert(error);
        LOG("[%s] %s\n", TAG, error.c_str());
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

TaskHandle_t wifiTask;
void wifi_loop ( void * pvParameters ) {   
    while (wifi_manager) { 
      wifi_manager->handle();
      if (!wifi_manager->Connected()) {
        if (settings_manager->settings.reboot_timeout) {
          if (!reboot_timeout) reboot_timeout = millis() + (settings_manager->settings.reboot_timeout * 60000);
          else if (millis() > reboot_timeout) ESP.restart();
        }
      } else reboot_timeout = 0;
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
  audioLogger = &Serial;
  /* Hardware setup */
  gpio_reset_pin(led_status);
  gpio_set_direction(led_status, GPIO_MODE_OUTPUT);
  gpio_set_pull_mode(led_status, GPIO_FLOATING);

  gpio_reset_pin(led_indicator);
  gpio_set_direction(led_indicator, GPIO_MODE_OUTPUT);
  gpio_set_pull_mode(led_indicator, GPIO_FLOATING);
  ledcSetup(0, 500, 8);
  ledcAttachPin(led_indicator, 0);

  gpio_reset_pin(detect_line);
  gpio_set_direction(detect_line, GPIO_MODE_INPUT);
  gpio_set_pull_mode(detect_line, GPIO_PULLUP_ONLY);

  gpio_reset_pin(button_boot);
  gpio_set_direction(button_boot, GPIO_MODE_INPUT);
  gpio_set_pull_mode(button_boot, GPIO_FLOATING);

  gpio_reset_pin(relay_line);
  gpio_set_direction(relay_line, GPIO_MODE_OUTPUT);
  gpio_set_pull_mode(relay_line, GPIO_FLOATING);

  gpio_reset_pin(switch_open);
  gpio_set_direction(switch_open, GPIO_MODE_OUTPUT);
  gpio_set_pull_mode(switch_open, GPIO_FLOATING);

  gpio_reset_pin(switch_phone);
  gpio_set_direction(switch_phone, GPIO_MODE_OUTPUT);
  gpio_set_pull_mode(switch_phone, GPIO_FLOATING);

  audioOut->SetOutputModeMono(true);
  timer0 = timerBegin(0, 80, true); // 12,5 ns * 80 = 1000ns = 1us
  timerAttachInterrupt(timer0, &TimerHandler0, false); //edge interrupts do not work, use false
  timerAlarmWrite(timer0, 50000, true);
  timerAlarmEnable(timer0);

  /* WatchDog configure*/
  esp_task_wdt_deinit();
  esp_task_wdt_init(WDT_TIMEOUT * 1000, true);
  esp_task_wdt_add(NULL);

  /* System startup */

  device_status.line_status = l_status_close;
  LOG("[%s] %s\n", TAG, CONFIG_CHIP_DEVICE_PRODUCT_NAME);
  LOG("[%s] %s\n", TAG, CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION);
  LOG("[%s] %s\n", TAG, "System setup");
  if (!aFS.begin(true)) {
    LOG("[%s] %s\n", TAG, "An Error has occurred while mounting file system.");
    hw_status.last_error = 6;
    return;
  }
  LOG("[%s] %s\n", TAG, "Settings init");
  settings_manager = new SettingsManager(SETTING_FILENAME);
  settings_manager->LoadSettings(aFS);
  if (currentAction == WAIT) gpio_set_level(relay_line, !settings_manager->settings.address_counter && settings_manager->settings.phone_disable);

  pcnt_config_t pcnt_config = {
      .pulse_gpio_num = detect_line,
      .ctrl_gpio_num = PCNT_PIN_NOT_USED,
      .lctrl_mode = PCNT_MODE_KEEP,
      .hctrl_mode = PCNT_MODE_KEEP,
      .pos_mode = PCNT_COUNT_DIS,
      .neg_mode = PCNT_COUNT_INC,
      .counter_h_lim = 256,
      .counter_l_lim = 0,
      .unit = PCNT_UNIT_0,
      .channel = PCNT_CHANNEL_0,
  };
  pcnt_unit_config(&pcnt_config);
  pcnt_set_filter_value(PCNT_UNIT_0, settings_manager->settings.impulse_filter);
  pcnt_filter_enable(PCNT_UNIT_0);
  pcnt_counter_pause(PCNT_UNIT_0);
  pcnt_counter_clear(PCNT_UNIT_0);
  pcnt_counter_resume(PCNT_UNIT_0);
  attachInterrupt(detect_line, call_detector_isr, CHANGE);

  LOG("[%s] %s\n", TAG, "WiFi init");
  wifi_manager = new WiFiManager(settings_manager->settings.wifi_ssid, settings_manager->settings.wifi_passwd);
  if (!hw_status.web_services_init) web_server_init();
  LOG("[%s] %s\n", TAG, "Web services init complete");
  xTaskCreatePinnedToCore(wifi_loop, "WiFi infinity loops", 4096, NULL, tskIDLE_PRIORITY,  &wifiTask, tskNO_AFFINITY);
  LOG("[%s] %s\n", TAG, "WiFi monitor started");
  device_info = new DevInfo;
  device_info->name = CONFIG_CHIP_DEVICE_PRODUCT_NAME;
  device_info->manufacturer = "SCratORS © SCHome (SmartHome Devices)";
  device_info->product = CONFIG_CHIP_DEVICE_PRODUCT_NAME;
  device_info->firmware = CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION;
  device_info->control = "http://"+wifi_manager->ip;
  std::string txt = WiFi.macAddress().c_str();
  size_t index;
  while ((index = txt.find(":")) != std::string::npos) txt.replace(index, 1, "");
  LOG("[%s] MQTT ID: %s\n", TAG, txt.c_str());
  device_info->mqtt_entity_id = txt;
  device_info->dev_name = settings_manager->settings.dev_name;
  LOG("[%s] %s\n", TAG, "System started.");
}

void loop() {
  esp_task_wdt_reset();
  if (wifi_manager->Connected()) {
      if (!hw_status.time_configure) {
        ws.textAll(getStatus().c_str());
        sendAlert("Соединение с Wi-Fi Установлено!\nАдрес устройства: " + wifi_manager->ip);
        enable_syslog(settings_manager->settings.syslog);
        enable_mqtt(settings_manager->settings.server_type == 1);
        enable_tlg(settings_manager->settings.server_type == 2);
        tlg_restart_timer = millis();
        hw_status.time_configure = true; 
      } else {
        if (settings_manager->settings.server_type == 2) {
          if (!tlg_manager) {
            if (millis() - tlg_restart_timer > 120000) {
              LOG("[%s] Restart telegram client.\n", TAG);
              enable_tlg(settings_manager->settings.server_type == 2);
              tlg_restart_timer = millis();
            }
          }
        }
      }
  } else {
    if (hw_status.time_configure) {
      enable_syslog(false);
      enable_mqtt(false);
      enable_tlg(false);
      hw_status.time_configure = false;
    }
  }
 
/*
  if (currentAction != WAIT) doAction(millis()-detectMillis); 
  else if (settings_manager->settings.address_counter) {
    if (syncCounter) {
      if (millis() - reset_time > 190) {
        pcnt_get_counter_value(PCNT_UNIT_0, &currentAddressCounter);
        if (currentAddressCounter) {
          triggerCounter = settings_manager->settings.address_counter == currentAddressCounter;
          if (triggerCounter) {
            gpio_set_level(relay_line, settings_manager->settings.phone_disable); 
            calling_detect();
          }
          LOG("[%s] Select address: %d, counter: %d\n", TAG, settings_manager->settings.address_counter, currentAddressCounter);
        }
        syncCounter = false;
      }
    }
  }
*/
  if (currentAction != WAIT) doAction(millis()-detectMillis); 
  else if (settings_manager->settings.address_counter) {
    if (syncCounter && !finishCounter) {
      if (millis() - reset_time > settings_manager->settings.counter_duration) {
        pcnt_get_counter_value(PCNT_UNIT_0, &currentAddressCounter);
        finishCounter = true;
        if (currentAddressCounter) {
          triggerCounter = settings_manager->settings.address_counter == currentAddressCounter;
          if (triggerCounter) {
            gpio_set_level(relay_line, settings_manager->settings.phone_disable); 
            calling_detect();
          }
          LOG("[%s] Select address: %d, counter: %d\n", TAG, settings_manager->settings.address_counter, currentAddressCounter);
        } else LOG("[%s] Select address: %d, counter: not detect\n", TAG, settings_manager->settings.address_counter);
      }
    } else if (finishCounter) { //Если досчитали
      if (millis() - reset_time > settings_manager->settings.counter_duration) { //Если прошло немного времени, то скидываем флаги, для новых подвигов
        syncCounter = false;
        finishCounter = false;
      }
    }
  }


if (!settings_manager->settings.child_lock) {
    bool btnState = !gpio_get_level(button_boot);
    if (btnState && !btnPressFlag && millis() - last_toggle > DEBOUNCE_DELAY) {
        btnPressFlag = true;
        last_toggle = millis();
        if (currentAction == CALLING) setAccept(settings_manager->settings.accept_call = true);
        else {
          if (hw_status.last_error) hw_status.last_error = 0;
          else if (settings_manager->last_error) settings_manager->last_error = 0;
          else if (wifi_manager && wifi_manager->last_error) wifi_manager->last_error = 0;
          else setAccept(!settings_manager->settings.accept_call);
        } 
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