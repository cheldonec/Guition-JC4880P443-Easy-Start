# 🖥️ ESP32-P4 GUI Project — Guition JC4880P443 Easy Start  
**Stable project with Wi-Fi, SNTP, audio, LVGL 9, and OTA via RCP/C6**

[![ESP-IDF v5.5.2+](https://img.shields.io/badge/ESP--IDF-5.5.2+-blue.svg)](https://github.com/espressif/esp-idf)
[![LVGL v9](https://img.shields.io/badge/LVGL-v9-orange.svg)](https://lvgl.io/)
[![License](https://img.shields.io/github/license/espressif/esp-idf.svg)](LICENSE)

---

## 🖼️ Overview

This project implements a **feature-rich embedded GUI** for the **Guition JC4880P443** development board (ESP32-P4 + MIPI DSI LCD + Touch + Audio), with:

✅ **Hardware modules**  
- LCD: **ST7701** (480×800, 16-bit RGB565, MIPI DSI, 2 lanes)  
- Touch: **GT911** (I²C)  
- Audio: **ES8311** (codec, I²S, I²C)  
- Storage: **SD card** (SPI/SDIO)  
- External RCP: **ESP32-C6** (OTA via `slave_fw` partition)  

✅ **Software**  
- **Centrally managed events** (`project_event_handler_manager`)  
- **Wi-Fi STA** with NVS credential caching (11 buckets)  
- **SNTP time sync** with fallback and hourly persistence  
- **Memory monitor** (DRAM/PSRAM/Stack)  
- **LVGL 9** with MIPI DSI + GT911 touch  
- **Audio playback/recording** (file/memory)  
- **Customizable board configuration** via `board_config.h`  

✅ **Ready for OTA**  
- Host OTA via `slave_fw` partition  
- Automatic `network_adapter.bin` copy on build  

---

## 📦 Hardware: Guition JC4880P443 Pinout

| Component | Chip / Module | Interface | GPIO / Pin | Notes |
|-----------|---------------|-----------|------------|-------|
| **LCD** | ST7701 | MIPI DSI | — | 480×800, RGB565, 2 lanes, BL on GPIO23 |
| **Touch** | GT911 | I²C | SDA: GPIO7, SCL: GPIO8 | INT/RST unused (GPIO_NC) |
| **Audio Codec** | ES8311 | I²S + I²C | MCLK: GPIO13, WS: GPIO10, BCLK: GPIO12, DIN: GPIO48, DOUT: GPIO9, I²C: GPIO7/GPIO8 | PA: GPIO11 |
| **SD Card** | — | SPI/SDIO | — | Uses default SPI host (SCK: GPIO36, etc.) |
| **External RCP** | ESP32-C6 | SDIO | — | Firmware: `network_adapter.bin` → `slave_fw` @ `0x620000` |

> ℹ️ All pins are described in [`board_config.h`](main/board_config.h). Change them to match your board.

---

## 🚀 Quick Start

### 1. Clone & Setup

```bash
# Clone the project
git clone https://github.com/your-org/esp32-p4-guition-jc4880p443.git
cd esp32-p4-guition-jc4880p443

# Set Target
idf.py set-target esp32p4

# Build project
idf.py build

# Flash 
idf.py flash

# Flash RCP firmware manually to partition
🔥 RCP OTA: After first flash, network_adapter.bin is auto-copied to main/applications/sys_app_rcp_c6_update/slave_fw_bin/1_network_adapter.bin.

python flash_slave_fw.py

--------------------------------------------------------------------------------------------------------------------------

📁 Project Structure
sample_project/
├── main/
│   ├── board_config.h              # Hardware pinout & settings
│   ├── board_init.c/h              # Unified init/deinit of all modules
│   ├── main.c                      # Entry point
│   ├── main_sys_event_handler.c    # Centralized event logger
│   │
│   ├── onboard_hardware/           # Hardware drivers (modular)
│   │   ├── i2c_if/
│   │   ├── i2s_if/
│   │   ├── audio_module/           # ES8311 driver
│   │   ├── screen/                 # ST7701 MIPI LCD
│   │   ├── touch/                  # GT911
│   │   ├── storage/                # SD card I/O
│   │   └── net_if/                 # Wi-Fi & netif init
│   │
│   ├── applications/               # Feature modules (event-driven)
│   │   ├── sys_app_wifi_manager/
│   │   ├── sys_app_time_sync/
│   │   ├── sys_app_memory_monitor/
│   │   ├── sys_app_audio_play_and_rec/
│   │   ├── sys_app_rcp_c6_update/  # OTA via slave_fw
│   │   └── sys_applications.c      # Top-level init of apps
│   │
│   ├── project_event_handler_manager/  # Core event system
│   └── lvgl_gui_app/               # LVGL 9 integration
│
├── flash_slave_fw.py               # Helper script for RCP OTA
├── network_adapter.bin             # [optional] RCP firmware (put here before build)
└── README.md

--------------------------------------------------------------------------------------------------------------------------

🌐 Wi-Fi & Time Sync
Wi-Fi Caching
Saves up to 11 networks (bucket 0 = last used, buckets 1–10 = history)
Automatic fallback: bucket 0 → 1..10 → reserv_ssid → loop
Credentials saved after successful connection
Time Sync (SNTP)
Syncs on IP_EVENT_STA_GOT_IP or from time_sync_task
Falls back to NVS time if no Wi-Fi on boot
Saves time to NVS every hour (time_keep_alive_task)


--------------------------------------------------------------------------------------------------------------------------

📊 Logging & Event System
Event Types (see APP_EVENT_* in project_event_handler_manager.h)
Event	Payload	Logged Example
APP_EVENT_WIFI_SCAN_DONE	wifi_scan_result_sys_msg_t	Found 6 Wi-Fi networks → ASCII table with RSSI, Auth, BSSID
APP_EVENT_WIFI_CONNECTED	ip, ssid	192.168.2.77 (SSID=Keenetic-4201)
APP_EVENT_TIME_SYNCED	timestamp, from_sntp	2026-07-03 07:47:34 (from SNTP=true)
APP_EVENT_MEMORY_STATUS	free_dram_kb, min_dram_kb, largest_dram_kb, free_psram_mb, min_psram_mb, largest_psram_mb, stack_words	DRAM: 184 KiB (188416 B) | PSRAM: 25 MiB (26214400 B) | Stack: 3580 words
APP_EVENT_AUDIO_PLAY_COMPLETE	void * (file path)	✅ Audio playback finished: sd:/audio/test.wav (if implemented)

🔍 Centralized logger:

All events are logged in main_sys_event_handler.c. You can extend it to trigger UI updates, voice feedback, or low-memory alerts.


🔧 APP_EVENT_MEMORY_STATUS Deep Dive
C
// main/project_event_handler_manager/project_event_handler_manager.h
typedef struct {
    uint32_t free_dram_kb;   // ✅ Current free DRAM (KB)
    uint32_t min_dram_kb;    // ⚠️ Minimum free DRAM (since boot)
    uint32_t largest_dram_kb; // ✅ Largest contiguous free block (KB)
    uint32_t free_psram_mb;  // ✅ Current free PSRAM (MB)
    uint32_t min_psram_mb;   // ⚠️ Minimum free PSRAM (since boot)
    uint32_t largest_psram_mb; // ✅ Largest contiguous free block (MB)
    uint32_t stack_words;    // ✅ High water mark (words, not bytes)
} memory_status_sys_msg_t;
🔹 How it works
Created in memory_monitor_app.c
Runs as app_mem_mon_task (priority=1, stack=4096)
Sends event every period_sec (default=10s)
Logs warnings if free_dram < 50 KB
Logs full memory status in main_sys_event_handler as:
[MEMORY] DRAM: 184 KiB (188416 B) | PSRAM: 25 MiB (26214400 B) | Stack: 3580 words

🔹 Use cases
📉 Detect memory leaks (watch min_dram_kb decreasing over time)
💥 Avoid allocation failures (check largest_dram_kb before big allocations)
⏰ Optimize task stacks (stack_words shows real usage)

💡 Tip: For debug builds, enable CONFIG_HEAP_STACK_CHECK=y in sdkconfig to catch stack overflows.


🎯 Summary
Feature	Benefit
✅ Centralized logging	Single place for all system events
✅ Extensible	Add new events in project_event_handler_manager.h
✅ Type-safe payloads	Structured messages with compile-time checks
✅ Low memory alerts	Automatic warnings at <50 KiB DRAM

All events are logged in main_sys_event_handler.c. You can extend it to trigger UI updates or voice feedback.

--------------------------------------------------------------------------------------------------------------------------

🔊 Audio (WIP)
Playback: from SD card or memory (wav/pcm)
Recording: to SD card or memory
Rate: 16 kHz, mono/stereo

🎤 Future plans: Voice feedback (TTS) or STT (Yandex Cloud) — see ya_cloud_speech_recognition/

---------------------------------------------------------------------------------------------------------------------------

🔄 OTA via RCP (ESP32-C6)
Prerequisites

network_adapter.bin must be in project root

(Built automatically if esp-hosted is available)



Partition table includes slave_fw region


CSV
# ESP-IDF partition table (2.5M app, 0.5M spiffs, 0.5M slave_fw)
nvs,      data, nvs,     0x9000, 0x6000,
otadata,  data, ota,     0xf000, 0x2000,
phy_init, data, phy,     0x10000,0x1000,
ota_0,    app,  ota_0,   0x11000,0x300000,
ota_1,    app,  ota_1,   0x310000,0x300000,
slave_fw, data, slave_fw,0x610000,0x10000,  # ← network_adapter.bin → here

----------------------------------------------------------------------------------------------------------------------------


Made with ❤️ for ESP32-P4 + Guition JC4880P443
