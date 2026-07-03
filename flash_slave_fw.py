#!/usr/bin/env python3
# flash_slave_fw.py — прошивка network_adapter.bin в раздел slave_fw (с --force)

import os
import sys
import subprocess

# === Настройки ===
PORT = "COM9"
PARTITION_NAME = "slave_fw"
DESCRIPTION = "Network adapter firmware for external RCP/C6 module"


def run_command(cmd):
    """Выполняет команду и проверяет результат"""
    print(f"🔧 Выполняю: {' '.join(cmd)}")
    result = subprocess.run(cmd)
    if result.returncode != 0:
        print(f"❌ Ошибка при выполнении команды: {' '.join(cmd)}")
        sys.exit(1)


def find_esptool():
    """Ищет esptool.py"""
    if "IDF_PATH" in os.environ:
        candidates = [
            os.path.join(os.environ["IDF_PATH"], "components", "esptool_py", "esptool", "esptool.py"),
            os.path.join(os.environ["IDF_PATH"], "components", "esptool_py", "esptool", "__main__.py"),
            os.path.join(os.environ["IDF_PATH"], "tools", "esptool.py", "esptool.py"),
        ]
        for c in candidates:
            if os.path.isfile(c):
                return c

    for cmd in ["python", "-m", "esptool"]:
        try:
            subprocess.run([cmd, "--version"], capture_output=True, check=True)
            return ["python", "-m", "esptool"]
        except (subprocess.CalledProcessError, FileNotFoundError):
            continue

    return None


def main():
    print("🚀 Запуск прошивки раздела 'slave_fw'...\n")

    # 1. Получаем путь к проекту (где лежит main/)
    project_dir = os.path.dirname(os.path.abspath(__file__))
    target_bin = os.path.join(project_dir, "main", "applications", "sys_app_rcp_c6_update", "slave_fw_bin", "1_network_adapter.bin")

    if not os.path.exists(target_bin):
        print(f"❌ Файл не найден: {target_bin}")
        print("💡 Выполните: idf.py build (для копирования network_adapter.bin)")
        print("   Или вручную: cp network_adapter.bin main/applications/rcp_c6_update/slave_fw_bin/1_network_adapter.bin")
        sys.exit(1)

    print(f"📌 Файл: {target_bin}")

    # 2. ✅ ИСПРАВЛЕНО: Используем фиксированный offset 0x620000 (IDF рассчитал именно его)
    offset = 0x620000
    print(f"📌 Offset: 0x{offset:x} ({offset})")

    # 3. Ищем esptool.py
    esptool_path = find_esptool()
    if not esptool_path:
        print("❌ Не найден esptool.py")
        sys.exit(1)
    print(f"📌 esptool.py: {esptool_path}")

    # 4. Запускаем прошивку
    if isinstance(esptool_path, list):
        cmd = esptool_path + [
            "--port", PORT,
            "write_flash",
            "--force",
            hex(offset),
            target_bin
        ]
    else:
        cmd = [
            sys.executable, esptool_path,
            "--port", PORT,
            "write_flash",
            "--force",
            hex(offset),
            target_bin
        ]

    print(f"📌 Прошивка: {PARTITION_NAME}")
    print(f"   Описание: {DESCRIPTION}")
    print()

    run_command(cmd)

    print("✅ Раздел 'slave_fw' успешно прошит!")
    print("🎉 Готово!")


if __name__ == "__main__":
    main()