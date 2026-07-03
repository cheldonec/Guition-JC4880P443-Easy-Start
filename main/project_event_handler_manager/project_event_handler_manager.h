// main/project_event_handler/project_event_handler.h
#ifndef PROJECT_EVENT_HANDLER_H
#define PROJECT_EVENT_HANDLER_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_wifi.h"
#include "time.h"
#include "sys/time.h"

#ifdef __cplusplus
extern "C" {
#endif

// Массив обработчиков (упрощённый — можно расширить на хеш-таблицу или vector)
#define MAX_HANDLERS 64

//APP_EVENT_AUDIO_PLAY_VOLUME_CHANGED
typedef struct {
    int new_percent;             // новое значение громкости (0..100)
} audio_volume_changed_sys_msg_t;

//APP_EVENT_AUDIO_START_REC_TO_FILE
typedef struct {
    uint32_t duration_sec;
    const char *filename;
} audio_start_rec_to_file_sys_msg_t;

// APP_EVENT_AUDIO_STOPPED_REC_TO_FILE_USER
typedef struct {
    uint32_t size;
    const char *filename;
} audio_stopped_rec_to_file_user_sys_msg_t;

//APP_EVENT_AUDIO_DONE_REC_TO_FILE
typedef struct {
    uint32_t size;               // размер записи (байт)
    const char *filename;
} audio_done_rec_to_file_sys_msg_t;

//APP_EVENT_AUDIO_START_REC_TO_MEM
typedef struct {
    uint32_t duration_sec;
} audio_start_rec_to_mem_sys_msg_t;

// APP_EVENT_AUDIO_STOPPED_REC_TO_MEM_USER
typedef struct {
    uint32_t bytes_recorded; // сколько успели записать до стопа
    uint8_t *data;
} audio_stopped_rec_to_mem_user_sys_msg_t;

// APP_EVENT_AUDIO_DONE_REC_TO_MEM
typedef struct {
    uint32_t size;               // размер записи (байт)
    const uint8_t *data;         // ← копия данных (в heap), user должен free()!
} audio_done_rec_to_mem_sys_msg_t;

//APP_EVENT_AUDIO_START_PLAY_FROM_FILE
typedef struct {
    const char *filename;
} audio_start_play_from_file_sys_msg_t;

//APP_EVENT_AUDIO_STOPPED_PLAY_FILE_USER
typedef struct {
    const char *filename;
} audio_stopped_play_file_user_sys_msg_t;

//APP_EVENT_AUDIO_DONE_PLAY_FROM_FILE
typedef struct {
    const char *filename;
} audio_done_play_from_file_sys_msg_t;

//APP_EVENT_AUDIO_START_PLAY_FROM_MEM
typedef struct {
    const uint8_t *buffer;
    uint32_t size;
}audio_start_play_from_mem_sys_msg_t;

//APP_EVENT_AUDIO_STOPPED_PLAY_MEM_USER
typedef struct {
    uint32_t size;
} audio_stopped_play_mem_user_sys_msg_t;

//APP_EVENT_AUDIO_DONE_PLAY_FROM_MEM
typedef struct {
    uint32_t size;
}audio_done_play_from_mem_sys_msg_t;

//APP_EVENT_SD_CARD_MOUNTED
typedef struct {
    const char *mount_point;
} audio_sd_card_mounted_sys_msg_t;

//APP_EVENT_SD_CARD_UNMOUNTED
typedef struct {
    // пустая структура — достаточно одного события
} audio_sd_card_unmounted_sys_msg_t;

//APP_EVENT_WIFI_CONNECTED
typedef struct {
    esp_ip4_addr_t ip;
    char ssid[32];
} wifi_connected_sys_msg_t;

//APP_EVENT_WIFI_SCAN_DONE
typedef struct {
    uint16_t ap_count;
    wifi_ap_record_t ap_records[0]; // flexible array member (C99)
} wifi_scan_result_sys_msg_t;

//APP_EVENT_WIFI_DISCONNECTED
typedef struct {
    // empty — можно добавить reason код в будущем
} wifi_disconnected_sys_msg_t;


// SYSTEM TIME MESSAGES
typedef struct {
    time_t timestamp;     // Unix timestamp (секунды с 1970-01-01)
    bool from_sntp;       // true = синхронизировано через SNTP, false = восстановлено из NVS
} time_synced_sys_msg_t;

// --- MEMORY messages ---
typedef struct {
    size_t free_dram_kb;         // свободная DRAM (KiB)
    size_t min_dram_kb;          // минимально свободная DRAM (KiB)
    size_t largest_dram_kb;      // largest free block in DRAM (KiB)

    size_t free_psram_mb;        // свободная PSRAM (MiB)
    size_t min_psram_mb;         // минимально свободная PSRAM (MiB)
    size_t largest_psram_mb;     // largest free block in PSRAM (MiB)

    UBaseType_t stack_words;     // остаток стека (в словах)
} memory_status_sys_msg_t;

// --- ID событий (пример — расширяется по мере необходимости) ---
typedef enum {
    PROJECT_EVENT_BASE = 0x0000,
    
    // System control events
    APP_EVENT_REQUEST_WIFI_SCAN = 0x0100,  // ← НОВОЕ: запрос сканирования
    APP_EVENT_REQUEST_WIFI_DISCONNECT,   // ← НОВОЕ
    APP_EVENT_REQUEST_WIFI_RECONNECT,
    
    // Wi-Fi events
    APP_EVENT_WIFI_SCAN_DONE,                         //wifi_scan_result_sys_msg_t;
    APP_EVENT_WIFI_CONNECTED,                         //wifi_connected_sys_msg_t;        
    APP_EVENT_WIFI_DISCONNECTED,                      //wifi_disconnected_sys_msg_t;

    // TIME events
    APP_EVENT_TIME_SYNCED,                             // time_synced_sys_msg_t
    
    // MEMORY events
    APP_EVENT_MEMORY_STATUS,                           // memory_status_sys_msg_t

    // Audio events
    APP_EVENT_AUDIO_PLAY_VOLUME_CHANGED,               //audio_volume_changed_sys_msg_t;
    APP_EVENT_AUDIO_START_REC_TO_FILE,                 //audio_start_rec_to_file_sys_msg_t; 
    APP_EVENT_AUDIO_STOPPED_REC_TO_FILE_USER,          //audio_stopped_rec_to_file_user_sys_msg_t
    APP_EVENT_AUDIO_DONE_REC_TO_FILE,                  //audio_done_rec_to_file_sys_msg_t; 

    APP_EVENT_AUDIO_START_REC_TO_MEM,                  //audio_start_rec_to_mem_sys_msg_t; 
    APP_EVENT_AUDIO_STOPPED_REC_TO_MEM_USER,           //audio_stopped_rec_to_mem_user_sys_msg_t;   
    APP_EVENT_AUDIO_DONE_REC_TO_MEM,                   //audio_done_rec_to_mem_sys_msg_t; 

    APP_EVENT_AUDIO_START_PLAY_FROM_FILE,              //audio_start_play_from_file_sys_msg_t; 
    APP_EVENT_AUDIO_DONE_PLAY_FROM_FILE,               //audio_done_play_from_file_sys_msg_t;   
    APP_EVENT_AUDIO_STOPPED_PLAY_FILE_USER,            //audio_stopped_play_file_user_sys_msg_t;   

    APP_EVENT_AUDIO_START_PLAY_FROM_MEM,               //audio_start_play_from_mem_sys_msg_t;   
    APP_EVENT_AUDIO_DONE_PLAY_FROM_MEM,                //audio_done_play_from_mem_sys_msg_t;   
    APP_EVENT_AUDIO_STOPPED_PLAY_MEM_USER,             //audio_stopped_play_mem_user_sys_msg_t;   

    // SD Card events
    APP_EVENT_SD_CARD_MOUNTED,                         //audio_sd_card_mounted_sys_msg_t
    APP_EVENT_SD_CARD_UNMOUNTED,                       //audio_sd_card_unmounted_sys_msg_t
    
    // LCD / Touch events
    APP_EVENT_TOUCH_TAP,
    APP_EVENT_LCD_BACKLIGHT_ON,
    APP_EVENT_LCD_BACKLIGHT_OFF,
    
    // System events
    APP_EVENT_SYSTEM_POWER_LOW,
    APP_EVENT_SYSTEM_SHUTDOWN,
    
    // Custom user events
    APP_EVENT_USER_0 = 0xF000,
    APP_EVENT_USER_1,
    APP_EVENT_USER_2,
    APP_EVENT_USER_3,
    
    PROJECT_EVENT_MAX = 0xFFFF,
} project_event_id_t;

// --- Структура события ---
typedef struct {
    project_event_id_t id;     // Идентификатор события
    void *data;                // Указатель на данные (должен быть heap-буфером)
    uint16_t data_len;         // Размер данных в байтах
} project_event_t;

// --- Callback для обработки событий ---
typedef void (*project_event_cb_t)(project_event_id_t id, const void *data, uint16_t len);

// --- API ---
esp_err_t project_event_init(void);
esp_err_t project_event_send(project_event_id_t id, const void *data, uint16_t data_len);
esp_err_t project_event_register_handler(project_event_id_t id, project_event_cb_t cb);
esp_err_t project_event_unregister_handler(project_event_id_t id, project_event_cb_t cb);
void project_event_handler(void *arg);

#ifdef __cplusplus
}
#endif

#endif // PROJECT_EVENT_HANDLER_H