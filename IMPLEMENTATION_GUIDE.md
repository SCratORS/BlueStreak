# Инструкция по реализации одновременной работы Telegram и MQTT

## Обзор

Этот гайд показывает точные изменения в коде `src/main.cpp` для включения одновременного управления через Telegram и MQTT Home Assistant.

## Изменения в src/main.cpp

### 1. Функция `enable_mqtt()` (строка ~1077)

**БЫЛО:**
```cpp
void enable_mqtt(bool value) {
  if (value) {
    if (mqtt_manager) return;
    // ... код инициализации
  }
}
```

**СТАЛО:**
```cpp
void enable_mqtt(bool value) {
  if (value) {
    // Проверяем, что server_type = 1 (MQTT) или 3 (MQTT+Telegram)
    if (settings_manager->settings.server_type != 1 && 
        settings_manager->settings.server_type != 3) return;
    
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
```

### 2. Функция `enable_tlg()` (строка ~1055)

**БЫЛО:**
```cpp
void enable_tlg(bool value){
  if (value) {
    if (settings_manager->settings.tlg_token == "") return;
    if (tlg_manager) return;
    // ... код инициализации
  }
}
```

**СТАЛО:**
```cpp
void enable_tlg(bool value){
  if (value) {
    // Проверяем, что server_type = 2 (Telegram) или 3 (MQTT+Telegram)
    if (settings_manager->settings.server_type != 2 && 
        settings_manager->settings.server_type != 3) return;
    
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
```

### 3. Функция `save_settings()` (строка ~1121)

**БЫЛО:**
```cpp
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
```

**СТАЛО:**
```cpp
void save_settings(){
  settings_manager->SaveSettings(aFS);
  wifi_manager->setSSID(settings_manager->settings.wifi_ssid);
  wifi_manager->setPasswd(settings_manager->settings.wifi_passwd);
  
  // Отключаем все сервисы
  enable_mqtt(false);
  enable_tlg(false);
  enable_syslog(false);
  delay(1000);
  
  // Включаем согласно server_type
  enable_syslog(settings_manager->settings.syslog);
  
  // MQTT: включен при server_type = 1 или 3
  enable_mqtt(settings_manager->settings.server_type == 1 || 
              settings_manager->settings.server_type == 3);
  
  // Telegram: включен при server_type = 2 или 3
  enable_tlg(settings_manager->settings.server_type == 2 || 
             settings_manager->settings.server_type == 3);
}
```

### 4. Функция `wifi_loop()` (строка ~1365)

**БЫЛО:**
```cpp
void wifi_loop ( void * pvParameters ) {   
    while (wifi_manager) { 
      wifi_manager->handle();
      if (!wifi_manager->Connected()) {
        // ... reboot timeout logic
      } else reboot_timeout = 0;
      
      if (settings_manager->settings.server_type == 1 && mqtt_manager) 
        mqtt_manager->handle();
      
      if (settings_manager->settings.server_type == 2 && tlg_manager) {
        // ... telegram code timeout
      }
      
      if (settings_manager->settings.ftp && ftp_server) ftp_server->handleFTP();
      vTaskDelay(pdMS_TO_TICKS(10));    
    }
    vTaskDelete(wifiTask);
}
```

**СТАЛО:**
```cpp
void wifi_loop ( void * pvParameters ) {   
    while (wifi_manager) { 
      wifi_manager->handle();
      
      if (!wifi_manager->Connected()) {
        if (settings_manager->settings.reboot_timeout) {
          if (!reboot_timeout) reboot_timeout = millis() + (settings_manager->settings.reboot_timeout * 60000);
          else if (millis() > reboot_timeout) ESP.restart();
        }
      } else reboot_timeout = 0;
      
      // Обработка MQTT если включен (type 1 или 3)
      if ((settings_manager->settings.server_type == 1 ||
           settings_manager->settings.server_type == 3) && mqtt_manager) 
        mqtt_manager->handle();
      
      // Обработка Telegram если включен (type 2 или 3)
      if ((settings_manager->settings.server_type == 2 ||
           settings_manager->settings.server_type == 3) && tlg_manager) {
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
```

### 5. WebSocket обработчик `handleWebSocketMessage()` (строка ~1201)

Найти блок:
```cpp
if (doc["method"] == "setServerType") {
```

**БЫЛО:**
```cpp
if (doc["method"] == "setServerType") {
  ws.textAll(settings_manager->setServerType(doc["value"].as<uint8_t>()).c_str());
  enable_mqtt(settings_manager->settings.server_type == 1);
  enable_tlg(settings_manager->settings.server_type == 2);    
  return;
}
```

**СТАЛО:**
```cpp
if (doc["method"] == "setServerType") {
  ws.textAll(settings_manager->setServerType(doc["value"].as<uint8_t>()).c_str());
  
  // Включаем MQTT если type = 1 или 3
  enable_mqtt(settings_manager->settings.server_type == 1 ||
              settings_manager->settings.server_type == 3);
  
  // Включаем Telegram если type = 2 или 3
  enable_tlg(settings_manager->settings.server_type == 2 ||
             settings_manager->settings.server_type == 3);    
  return;
}
```

### 6. Функция `setup()` (строка ~1509)

Найти блок в конце setup() где вызываются enable_syslog, enable_mqtt, enable_tlg:

**БЫЛО:**
```cpp
enable_syslog(settings_manager->settings.syslog);
enable_mqtt(settings_manager->settings.server_type == 1);
enable_tlg(settings_manager->settings.server_type == 2);
```

**СТАЛО:**
```cpp
enable_syslog(settings_manager->settings.syslog);

// Включаем MQTT если type = 1 или 3
enable_mqtt(settings_manager->settings.server_type == 1 || 
            settings_manager->settings.server_type == 3);

// Включаем Telegram если type = 2 или 3
enable_tlg(settings_manager->settings.server_type == 2 || 
           settings_manager->settings.server_type == 3);
```

### 7. Функция `loop()` (строка ~1533)

Найти блок:
```cpp
if (!hw_status.time_configure) {
  // ... код первой настройки
  enable_syslog(settings_manager->settings.syslog);
  enable_mqtt(settings_manager->settings.server_type == 1);
  enable_tlg(settings_manager->settings.server_type == 2);
  // ...
}
```

**ИЗМЕНИТЬ НА:**
```cpp
if (!hw_status.time_configure) {
  ws.textAll(getStatus().c_str());
  sendAlert("Соединение с Wi-Fi Установлено!\nАдрес устройства: " + wifi_manager->ip);
  
  enable_syslog(settings_manager->settings.syslog);
  
  // Включаем MQTT если type = 1 или 3
  enable_mqtt(settings_manager->settings.server_type == 1 || 
              settings_manager->settings.server_type == 3);
  
  // Включаем Telegram если type = 2 или 3
  enable_tlg(settings_manager->settings.server_type == 2 || 
             settings_manager->settings.server_type == 3);
  
  tlg_restart_timer = millis();
  hw_status.time_configure = true; 
}
```

Также в любом else блоке где есть:
```cpp
if (settings_manager->settings.server_type == 2) {
```

**ИЗМЕНИТЬ НА:**
```cpp
// Перезапуск Telegram если включен (type 2 или 3)
if ((settings_manager->settings.server_type == 2 ||
     settings_manager->settings.server_type == 3)) {
  if (!tlg_manager) {
    if (millis() - tlg_restart_timer > 120000) {
      LOG("[%s] Restart telegram client.\n", TAG);
      enable_tlg(true);
      tlg_restart_timer = millis();
    }
  }
}
```

## Компиляция

### PlatformIO
```bash
cd BlueStreak
pio run -t upload
```

### Arduino IDE
1. Откройте `src/main.cpp`
2. Установите все библиотеки из `platformio.ini`
3. Выберите плату ESP32 Dev Module
4. Нажмите Upload

## Тестирование

1. Подключитесь к Web-интерфейсу
2. Установите `server_type = 3`
3. Настройте MQTT и Telegram
4. Сохраните и перезагрузите
5. Проверьте логи через Serial Monitor

Ожидаемые сообщения:
```
[MQTT] Connected to broker
[TLG] Telegram bot started
[TLG] Bot authorized
```

## Частые проблемы

### Не подключается MQTT
- Проверьте параметры подключения
- Убедитесь что `server_type = 1` или `3`
- Проверьте доступность MQTT брокера

### Не работает Telegram
- Проверьте токен бота
- Убедитесь что `server_type = 2` или `3`
- Проверьте интернет-соединение

### Нехватка памяти
- Увеличьте `STACK_SIZE` в settings_manager.h
- Отключите ненужные функции (FTP, Syslog)
- Используйте ESP32 с большим объемом RAM

---

**Важно:** Эти изменения требуют только модификации `src/main.cpp`. Другие файлы менять не нужно, так как `server_type` уже поддерживает uint8_t и может хранить значение 0-255.
