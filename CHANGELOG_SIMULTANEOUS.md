# Changelog: Одновременная работа Telegram и MQTT

## Версия 2.2.8-Simultaneous (22.12.2025)

### Новые возможности

✨ **Одновременное управление через Telegram и MQTT Home Assistant**

#### Изменения в настройках

Параметр `server_type` теперь поддерживает 4 значения:
- `0` - Не активен (оба протокола отключены)
- `1` - Только MQTT
- `2` - Только Telegram
- `3` - **MQTT + Telegram одновременно** 🎉

### Технические изменения

#### 1. src/main.cpp

**Функция `enable_mqtt(bool value)`:**
```cpp
void enable_mqtt(bool value) {
  if (value) {
    if (settings_manager->settings.server_type != 1 && 
        settings_manager->settings.server_type != 3) return;
    // ... инициализация MQTT
  }
}
```

**Функция `enable_tlg(bool value)`:**
```cpp
void enable_tlg(bool value){
  if (value) {
    if (settings_manager->settings.server_type != 2 && 
        settings_manager->settings.server_type != 3) return;
    // ... инициализация Telegram
  }
}
```

**Функция `wifi_loop()`:**
```cpp
while (wifi_manager) { 
  wifi_manager->handle();
  // Обработка MQTT если включен (type 1 или 3)
  if ((settings_manager->settings.server_type == 1 ||
       settings_manager->settings.server_type == 3) && mqtt_manager) 
    mqtt_manager->handle();
  
  // Обработка Telegram если включен (type 2 или 3)
  if ((settings_manager->settings.server_type == 2 ||
       settings_manager->settings.server_type == 3) && tlg_manager) {
    // ... обработка Telegram
  }
}
```

**Функция `save_settings()`:**
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
  
  // Включаем согласно настройкам
  enable_syslog(settings_manager->settings.syslog);
  enable_mqtt(settings_manager->settings.server_type == 1 || 
              settings_manager->settings.server_type == 3);
  enable_tlg(settings_manager->settings.server_type == 2 || 
             settings_manager->settings.server_type == 3);
}
```

**Функция `setup()`:**
```cpp
void setup() {
  // ... инициализация
  enable_syslog(settings_manager->settings.syslog);
  enable_mqtt(settings_manager->settings.server_type == 1 || 
              settings_manager->settings.server_type == 3);
  enable_tlg(settings_manager->settings.server_type == 2 || 
             settings_manager->settings.server_type == 3);
}
```

**WebSocket обработчик:**
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

### Как использовать

1. **Через Web-интерфейс:**
   - Откройте настройки устройства
   - В разделе "Тип сервера" выберите значение `3`
   - Настройте параметры MQTT (сервер, порт, логин, пароль)
   - Настройте параметры Telegram (токен бота, ID пользователя)
   - Сохраните настройки

2. **Через API:**
```bash
curl "http://YOUR_DEVICE_IP/api?server_type=3&save=true"
```

3. **Через WebSocket:**
```json
{
  "method": "setServerType",
  "value": 3
}
```

### Преимущества

✅ Получайте уведомления о вызовах в Telegram
✅ Управляйте домофоном через Home Assistant
✅ Автоматизируйте открытие двери в HA
✅ Используйте голосовых ассистентов (Алиса, Google, Alexa)
✅ Получайте статистику в Home Assistant
✅ Быстрые команды через Telegram-бот

### Совместимость

- ESP32 (все варианты)
- Home Assistant с MQTT интеграцией
- Telegram Bot API
- PlatformIO / Arduino IDE

### Тестирование

Протестировано на:
- ESP32-WROOM-32
- Home Assistant 2025.12
- Mosquitto MQTT Broker 2.0.18
- Telegram Bot API (актуальная версия)

### Известные ограничения

⚠️ При одновременной работе двух протоколов увеличивается нагрузка на ESP32:
- Рекомендуемая частота CPU: 240 MHz
- Минимум свободной памяти: 64 KB
- При низком качестве Wi-Fi сигнала возможны задержки

### Откат к предыдущей версии

Чтобы вернуться к режиму с одним протоколом:
- Установите `server_type = 1` (только MQTT)
- Или `server_type = 2` (только Telegram)
- Сохраните настройки и перезагрузите устройство

---

**Автор:** dimasikaeger  
**Дата:** 22 декабря 2025  
**Базируется на:** BlueStreak 2.2.7 by SCratORS
