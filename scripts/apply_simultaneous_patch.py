#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Скрипт автоматического применения изменений для одновременной работы Telegram и MQTT

Использование:
    python3 scripts/apply_simultaneous_patch.py

Скрипт автоматически изменит src/main.cpp
"""

import re
import sys
import os
from pathlib import Path

def apply_patch(file_path):
    """ Применяет изменения к main.cpp """
    
    print(f"[✓] Чтение {file_path}...")
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    original_content = content
    
    # 1. Изменение enable_mqtt()
    print("[1/7] Патч enable_mqtt()...")
    pattern1 = r'void enable_mqtt\(bool value\) \{\s+if \(value\) \{\s+if \(mqtt_manager\) return;'
    replacement1 = '''void enable_mqtt(bool value) {
  if (value) {
    // Проверяем, что server_type = 1 (MQTT) или 3 (MQTT+Telegram)
    if (settings_manager->settings.server_type != 1 && 
        settings_manager->settings.server_type != 3) return;
    
    if (mqtt_manager) return;'''
    content = re.sub(pattern1, replacement1, content)
    
    # 2. Изменение enable_tlg()
    print("[2/7] Патч enable_tlg()...")
    pattern2 = r'void enable_tlg\(bool value\)\{\s+if \(value\) \{\s+if \(settings_manager->settings\.tlg_token == ""\) return;'
    replacement2 = '''void enable_tlg(bool value){
  if (value) {
    // Проверяем, что server_type = 2 (Telegram) или 3 (MQTT+Telegram)
    if (settings_manager->settings.server_type != 2 && 
        settings_manager->settings.server_type != 3) return;
    
    if (settings_manager->settings.tlg_token == "") return;'''
    content = re.sub(pattern2, replacement2, content)
    
    # 3. Изменение save_settings()
    print("[3/7] Патч save_settings()...")
    pattern3 = r'enable_syslog\(settings_manager->settings\.syslog\);\s+enable_mqtt\(settings_manager->settings\.server_type == 1\);\s+enable_tlg\(settings_manager->settings\.server_type == 2\);'
    replacement3 = '''enable_syslog(settings_manager->settings.syslog);
  
  // MQTT: включен при server_type = 1 или 3
  enable_mqtt(settings_manager->settings.server_type == 1 || 
              settings_manager->settings.server_type == 3);
  
  // Telegram: включен при server_type = 2 или 3
  enable_tlg(settings_manager->settings.server_type == 2 || 
             settings_manager->settings.server_type == 3);'''
    content = re.sub(pattern3, replacement3, content)
    
    # 4. Изменение wifi_loop() - MQTT
    print("[4/7] Патч wifi_loop() MQTT...")
    pattern4 = r'if \(settings_manager->settings\.server_type == 1 && mqtt_manager\)'
    replacement4 = '''if ((settings_manager->settings.server_type == 1 ||
           settings_manager->settings.server_type == 3) && mqtt_manager)'''
    content = re.sub(pattern4, replacement4, content)
    
    # 5. Изменение wifi_loop() - Telegram
    print("[5/7] Патч wifi_loop() Telegram...")
    pattern5 = r'if \(settings_manager->settings\.server_type == 2 && tlg_manager\)'
    replacement5 = '''if ((settings_manager->settings.server_type == 2 ||
           settings_manager->settings.server_type == 3) && tlg_manager)'''
    content = re.sub(pattern5, replacement5, content)
    
    # 6. Изменение WebSocket handler setServerType
    print("[6/7] Патч WebSocket handler...")
    pattern6 = r'if \(doc\["method"\] == "setServerType"\) \{\s+ws\.textAll\(settings_manager->setServerType\(doc\["value"\]\.as<uint8_t>\(\)\)\.c_str\(\)\);\s+enable_mqtt\(settings_manager->settings\.server_type == 1\);\s+enable_tlg\(settings_manager->settings\.server_type == 2\);'
    replacement6 = '''if (doc["method"] == "setServerType") {
      ws.textAll(settings_manager->setServerType(doc["value"].as<uint8_t>()).c_str());
      
      // Включаем MQTT если type = 1 или 3
      enable_mqtt(settings_manager->settings.server_type == 1 ||
                  settings_manager->settings.server_type == 3);
      
      // Включаем Telegram если type = 2 или 3
      enable_tlg(settings_manager->settings.server_type == 2 ||
                 settings_manager->settings.server_type == 3);'''
    content = re.sub(pattern6, replacement6, content, flags=re.MULTILINE)
    
    # 7. Изменение loop() - перезапуск Telegram
    print("[7/7] Патч loop() Telegram restart...")
    pattern7 = r'if \(settings_manager->settings\.server_type == 2\) \{\s+if \(!tlg_manager\)'
    replacement7 = '''if ((settings_manager->settings.server_type == 2 ||
         settings_manager->settings.server_type == 3)) {
        if (!tlg_manager)'''
    content = re.sub(pattern7, replacement7, content)
    
    # Проверяем что изменения применены
    if content == original_content:
        print("[!] Предупреждение: Изменения не применены. Возможно патч уже применён?")
        return False
    
    # Создаём резервную копию
    backup_path = str(file_path) + '.backup'
    print(f"[✓] Создание резервной копии: {backup_path}")
    with open(backup_path, 'w', encoding='utf-8') as f:
        f.write(original_content)
    
    # Сохраняем изменённый файл
    print(f"[✓] Сохранение изменений...")
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(content)
    
    return True

def main():
    print("="*70)
    print("Автоматическое применение патча для одновременной работы")
    print("Telegram + MQTT Home Assistant")
    print("="*70)
    print()
    
    # Находим корневую директорию проекта
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    main_cpp = project_root / 'src' / 'main.cpp'
    
    if not main_cpp.exists():
        print(f"[✗] Ошибка: Файл {main_cpp} не найден!")
        print(f"    Запустите скрипт из корневой директории проекта")
        return 1
    
    try:
        if apply_patch(main_cpp):
            print()
            print("="*70)
            print("[✓] Патч успешно применён!")
            print("="*70)
            print()
            print("Для компиляции выполните:")
            print("  pio run")
            print()
            print("Для загрузки на устройство:")
            print("  pio run -t upload")
            print()
            print("Резервная копия сохранена в: src/main.cpp.backup")
            return 0
        else:
            print("[✗] Патч не был применён")
            return 1
    except Exception as e:
        print(f"[✗] Ошибка: {e}")
        return 1

if __name__ == '__main__':
    sys.exit(main())
