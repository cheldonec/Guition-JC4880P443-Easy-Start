#include "main_sys_event_handler.h"
#include "project_event_handler_manager/project_event_handler_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "time.h"
#include "sys/time.h"
#include "applications/sys_app_wifi_manager/wifi_manager_app.h"

static const char* TAG = "SYS_EVT_HANDLER";

// Обработчик — логирует ВСЕ события
static void main_sys_event_handler(project_event_id_t id, const void *data, uint16_t len)
{
    switch (id) {
        // === AUDIO EVENTS ===
        case APP_EVENT_AUDIO_PLAY_VOLUME_CHANGED: {
            const audio_volume_changed_sys_msg_t *msg = (const audio_volume_changed_sys_msg_t*)data;
            ESP_LOGI(TAG, "[AUDIO] Volume changed to %d%%", msg->new_percent);
            break;
        }

        case APP_EVENT_AUDIO_START_REC_TO_FILE: {
            const audio_start_rec_to_file_sys_msg_t *msg = (const audio_start_rec_to_file_sys_msg_t*)data;
            ESP_LOGI(TAG, "[AUDIO] Start recording to file: %s (%lu sec)", msg->filename, msg->duration_sec);
            break;
        }

        case APP_EVENT_AUDIO_STOPPED_REC_TO_FILE_USER: {
            const audio_stopped_rec_to_file_user_sys_msg_t *msg = (const audio_stopped_rec_to_file_user_sys_msg_t*)data;
            ESP_LOGW(TAG, "[AUDIO] File recording stopped: %s (%lu bytes)", msg->filename, msg->size);
            break;
        }

        case APP_EVENT_AUDIO_DONE_REC_TO_FILE: {
            const audio_done_rec_to_file_sys_msg_t *msg = (const audio_done_rec_to_file_sys_msg_t*)data;
            ESP_LOGI(TAG, "[AUDIO] File recorded: %s (%lu bytes)", msg->filename, msg->size);
            break;
        }

        case APP_EVENT_AUDIO_START_REC_TO_MEM: {
            const audio_start_rec_to_mem_sys_msg_t *msg = (const audio_start_rec_to_mem_sys_msg_t*)data;
            ESP_LOGI(TAG, "[AUDIO] Start recording to memory (%lu sec)", msg->duration_sec);
            break;
        }

        case APP_EVENT_AUDIO_STOPPED_REC_TO_MEM_USER: {
            const audio_stopped_rec_to_mem_user_sys_msg_t *msg = (const audio_stopped_rec_to_mem_user_sys_msg_t*)data;
            ESP_LOGW(TAG, "[AUDIO] Memory recording stopped: %lu bytes", msg->bytes_recorded);
            break;
        }

        case APP_EVENT_AUDIO_DONE_REC_TO_MEM: {
            const audio_done_rec_to_mem_sys_msg_t *msg = (const audio_done_rec_to_mem_sys_msg_t*)data;
            ESP_LOGI(TAG, "[AUDIO] Memory recorded: %lu bytes", msg->size);
            break;
        }

        case APP_EVENT_AUDIO_START_PLAY_FROM_FILE: {
            const audio_start_play_from_file_sys_msg_t *msg = (const audio_start_play_from_file_sys_msg_t*)data;
            ESP_LOGI(TAG, "[AUDIO] Start playing from file: %s", msg->filename);
            break;
        }

        case APP_EVENT_AUDIO_DONE_PLAY_FROM_FILE: {
            const audio_done_play_from_file_sys_msg_t *msg = (const audio_done_play_from_file_sys_msg_t*)data;
            ESP_LOGI(TAG, "[AUDIO] File played: %s", msg->filename);
            break;
        }

        case APP_EVENT_AUDIO_START_PLAY_FROM_MEM: {
            const audio_start_play_from_mem_sys_msg_t *msg = (const audio_start_play_from_mem_sys_msg_t*)data;
            ESP_LOGI(TAG, "[AUDIO] Start playing from mem (%lu bytes)", msg->size);
            break;
        }

        case APP_EVENT_AUDIO_DONE_PLAY_FROM_MEM: {
            const audio_done_play_from_mem_sys_msg_t *msg = (const audio_done_play_from_mem_sys_msg_t*)data;
            ESP_LOGI(TAG, "[AUDIO] Memory playback complete: %lu bytes", msg->size);
            break;
        }

        case APP_EVENT_AUDIO_STOPPED_PLAY_FILE_USER: {
            const audio_stopped_play_file_user_sys_msg_t *msg = (const audio_stopped_play_file_user_sys_msg_t*)data;
            ESP_LOGW(TAG, "[AUDIO] File playback stopped: %s", msg->filename);
            break;
        }

        case APP_EVENT_AUDIO_STOPPED_PLAY_MEM_USER: {
            const audio_stopped_play_mem_user_sys_msg_t *msg = (const audio_stopped_play_mem_user_sys_msg_t*)data;
            ESP_LOGW(TAG, "[AUDIO] Memory playback stopped: %lu bytes", msg->size);
            break;
        }

        // === SD CARD EVENTS ===
        case APP_EVENT_SD_CARD_MOUNTED: {
            const sd_card_mounted_sys_msg_t *msg = (const sd_card_mounted_sys_msg_t*)data;
            ESP_LOGI(TAG, "[SD CARD] SD CARD MOUNTED at: %s", msg->mount_point);
            break;
        }

        case APP_EVENT_SD_CARD_UNMOUNTED: {
            ESP_LOGW(TAG, "[SD CARD] SD CARD UNMOUNTED");
            break;
        }

        case APP_EVENT_SD_FILES_LIST: {
            if (len < sizeof(sd_files_list_sys_msg_t)) {
                ESP_LOGE(TAG, "[SD] Invalid SD_FILES_LIST len=%u", len);
                break;
            }
            
            const sd_files_list_sys_msg_t *msg = (const sd_files_list_sys_msg_t*)data;
            ESP_LOGI(TAG, "[SD] File list: %u files", msg->file_count);

            // Выводим красивую таблицу
            printf("\n");
            printf("╔════════════════════════════════════════════════════════════════════════════════════════════════════════════╗\n");
            printf("║ %-60s ║\n", "SD CARD FILE LIST");
            printf("╠════════════════════════════════════════════════════════════════════════════════════════════════════════════╣\n");

            for (uint16_t i = 0; i < msg->file_count; i++) {
                const char *filename = &msg->filenames[i][0];
                printf("║ %-60s ║\n", filename);
            }

            printf("╚════════════════════════════════════════════════════════════════════════════════════════════════════════════╝\n\n");
            
            // ✅ Теперь отправляем событие LVGL-приложению
            size_t lvgl_size = sizeof(sd_file_list_lvgl_event_t) + msg->file_count * 64;
            sd_file_list_lvgl_event_t *lvgl_msg = heap_caps_malloc(lvgl_size, MALLOC_CAP_DEFAULT);
            if (lvgl_msg) {
                lvgl_msg->file_count = msg->file_count;
                for (uint16_t i = 0; i < msg->file_count; i++) {
                    strcpy(&lvgl_msg->filenames[i][0], &msg->filenames[i][0]);
                }
                project_event_send(APP_EVENT_SD_FILE_LIST_LVGL, lvgl_msg, lvgl_size);
            }
            break;
        }

        // === Wi-Fi events (пример) ===
        case APP_EVENT_WIFI_SCAN_DONE: {
            if (len == 0) {
                ESP_LOGI(TAG, "[WIFI] Scan complete: 0 networks");
                return;
            }

            if (len < sizeof(wifi_scan_result_sys_msg_t)) {
                ESP_LOGE(TAG, "[WIFI] Invalid scan result len=%u", len);
                return;
            }

            const wifi_scan_result_sys_msg_t *scan_msg = (const wifi_scan_result_sys_msg_t*)data;

            uint16_t count = scan_msg->ap_count;
            const wifi_ap_record_t *records = scan_msg->ap_records;

            printf("\n");
            printf("╔════════════════════════════════════════════════════════════════════════════════════════════════════════════╗\n");
            printf("║ %-18s │ %-6s │ %-6s │ %-12s │ %-17s ║\n", "SSID", "RSSI", "Freq", "Auth", "BSSID");
            printf("╠════════════════════════════════════════════════════════════════════════════════════════════════════════════╣\n");

            for (int i = 0; i < count; i++) {
                const char* band = (records[i].primary <= 14) ? "2.4 GHz" : "5 GHz";
                const char* auth_str;
                switch (records[i].authmode) {
                    case WIFI_AUTH_OPEN:         auth_str = "OPEN"; break;
                    case WIFI_AUTH_WEP:          auth_str = "WEP"; break;
                    case WIFI_AUTH_WPA_PSK:      auth_str = "WPA_PSK"; break;
                    case WIFI_AUTH_WPA2_PSK:     auth_str = "WPA2_PSK"; break;
                    case WIFI_AUTH_WPA_WPA2_PSK: auth_str = "WPA/WPA2"; break;
                    case WIFI_AUTH_WPA3_PSK:     auth_str = "WPA3_PSK"; break;
                    case WIFI_AUTH_WPA2_WPA3_PSK: auth_str = "WPA2/WPA3"; break;
                    case WIFI_AUTH_WAPI_PSK:     auth_str = "WAPI_PSK"; break;
                    default:                     auth_str = "UNKNOWN"; break;
                }

                const char* ssid_str = (strlen((char*)records[i].ssid) == 0) ? "Hidden" : (const char*)records[i].ssid;

                uint8_t *bssid = records[i].bssid;
                char bssid_str[18];
                sprintf(bssid_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                        bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);

                printf("║ %-18s │ %-5d  │ %-7s │ %-12s │ %-17s ║\n",
                    ssid_str,
                    records[i].rssi,
                    band,
                    auth_str,
                    bssid_str);
            }

            printf("╚════════════════════════════════════════════════════════════════════════════════════════════════════════════╝\n\n");
            // ✅ ВАЖНО: data уже heap-буфер, но его очистит project_event_handler — не free()!
            break;
        }
        case APP_EVENT_WIFI_CONNECTED: {
            if (len != sizeof(wifi_connected_sys_msg_t)) {
                ESP_LOGE(TAG, "[Wi-Fi] Invalid CONNECTED msg len=%u (expected %zu)", len, sizeof(wifi_connected_sys_msg_t));
                break;
            }
            
            const wifi_connected_sys_msg_t *msg = (const wifi_connected_sys_msg_t*)data;

            ESP_LOGI(TAG, "[Wi-Fi] Wi-Fi connected: " IPSTR " (SSID=%s)", IP2STR(&msg->ip), msg->ssid);
            /*
            в виде текстовой строки:
            static char last_ip[16] = {0};
            esp_ip4_ntoa(&event->ip_info.ip, last_ip, sizeof(last_ip)); // "192.168.1.100"
            */

            // тестовое сканирование сети
            wifi_scan_networks();
            break;
        }

        case APP_EVENT_WIFI_DISCONNECTED: {
            ESP_LOGW(TAG, "[Wi-Fi] Wi-Fi disconnected");
            break;
        }

        case APP_EVENT_TIME_SYNCED: {
            if (len != sizeof(time_synced_sys_msg_t)) {
                ESP_LOGE(TAG, "[TIME] Invalid TIME_SYNCED msg len=%u (expected %zu)", len, sizeof(time_synced_sys_msg_t));
                break;
            }

            const time_synced_sys_msg_t *msg = (const time_synced_sys_msg_t*)data;

            struct tm timeinfo;
            localtime_r(&msg->timestamp, &timeinfo);
            char strftime_buf[64];
            strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);

            ESP_LOGI(TAG, "[TIME] Time synced: %s (from SNTP=%s)", strftime_buf,
                    msg->from_sntp ? "true" : "false");
            break;
        }

        case APP_EVENT_MEMORY_STATUS: {
            if (len != sizeof(memory_status_sys_msg_t)) {
                ESP_LOGE(TAG, "[MEMORY] Invalid MEMORY_STATUS msg len=%u (expected %zu)", len, sizeof(memory_status_sys_msg_t));
                break;
            }

            const memory_status_sys_msg_t *msg = (const memory_status_sys_msg_t*)data;

            ESP_LOGI(TAG, "[MEMORY] DRAM: %u KiB (%u B) | PSRAM: %u MiB (%u B) | Stack: %lu words",
                    msg->free_dram_kb, msg->free_dram_kb * 1024,
                    msg->free_psram_mb, msg->free_psram_mb * 1024 * 1024,
                    msg->stack_words);
            break;
        }
        default:
            // Фоллбэк для остальных событий
            ESP_LOGD(TAG, "[UNKNOWN] Event 0x%04X received (%d bytes)", (unsigned)id, len);
            break;
    }
}

void main_sys_event_handler_register_all(void)
{
    project_event_register_handler(APP_EVENT_AUDIO_PLAY_VOLUME_CHANGED, main_sys_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_START_REC_TO_FILE, main_sys_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_STOPPED_REC_TO_FILE_USER, main_sys_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_DONE_REC_TO_FILE, main_sys_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_START_REC_TO_MEM, main_sys_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_STOPPED_REC_TO_MEM_USER, main_sys_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_DONE_REC_TO_MEM, main_sys_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_START_PLAY_FROM_FILE, main_sys_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_DONE_PLAY_FROM_FILE, main_sys_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_START_PLAY_FROM_MEM, main_sys_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_DONE_PLAY_FROM_MEM, main_sys_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_STOPPED_PLAY_FILE_USER, main_sys_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_STOPPED_PLAY_MEM_USER, main_sys_event_handler);

    project_event_register_handler(APP_EVENT_SD_CARD_MOUNTED, main_sys_event_handler);
    project_event_register_handler(APP_EVENT_SD_CARD_UNMOUNTED, main_sys_event_handler);
    project_event_register_handler(APP_EVENT_SD_FILES_LIST, main_sys_event_handler);

    project_event_register_handler(APP_EVENT_WIFI_SCAN_DONE, main_sys_event_handler);
    project_event_register_handler(APP_EVENT_WIFI_CONNECTED, main_sys_event_handler);
    project_event_register_handler(APP_EVENT_WIFI_DISCONNECTED, main_sys_event_handler);

    project_event_register_handler(APP_EVENT_TIME_SYNCED, main_sys_event_handler);

    project_event_register_handler(APP_EVENT_MEMORY_STATUS, main_sys_event_handler);
}