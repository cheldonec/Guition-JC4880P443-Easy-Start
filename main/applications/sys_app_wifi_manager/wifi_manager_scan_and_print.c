// main/applications/wifi_manager/wifi_manager_scan_and_connect.c
#include "wifi_manager_app.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "wifi_manager_scan_and_print.h"
#include "project_event_handler_manager/project_event_handler_manager.h"

static const char *TAG = "scan";

// --- Глобальный mutex/семафор для асинхронного ожидания ---
static EventGroupHandle_t s_scan_done_event = NULL;

// --- Обработчик событий Wi-Fi (теперь он и собирает данные!) ---
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "✅ WIFI_EVENT_SCAN_DONE → collecting data");

        uint16_t ap_count = 0;
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
        ESP_LOGI(TAG, "Found %d Wi-Fi networks", ap_count);

        if (ap_count == 0) {
            ESP_LOGI(TAG, "No networks → sending empty result");
            project_event_send(APP_EVENT_WIFI_SCAN_DONE, NULL, 0);
            return;
        }

        // 🔧 ПРЕДПОЧИТАЕМО: выделяем буфер один раз и сразу в SPIRAM
        size_t msg_size = sizeof(wifi_scan_result_sys_msg_t) + ap_count * sizeof(wifi_ap_record_t);
        wifi_scan_result_sys_msg_t *scan_msg = heap_caps_malloc(msg_size, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
        if (!scan_msg) {
            ESP_LOGE(TAG, "❌ Failed to allocate %zu bytes for scan_msg", msg_size);
            return;
        }

        scan_msg->ap_count = ap_count;

        // Сканируем только уникальные по BSSID — ОПТИМАЛЬНО
        wifi_ap_record_t *ap_records = calloc(ap_count, sizeof(wifi_ap_record_t));
        if (!ap_records) {
            heap_caps_free(scan_msg);
            ESP_LOGE(TAG, "❌ Failed to allocate ap_records");
            return;
        }

        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));

        // Уникализация по BSSID
        wifi_ap_record_t *unique_records = calloc(ap_count, sizeof(wifi_ap_record_t));
        if (!unique_records) {
            heap_caps_free(scan_msg);
            free(ap_records);
            ESP_LOGE(TAG, "❌ Failed to allocate unique_records");
            return;
        }

        int unique_count = 0;
        for (int i = 0; i < ap_count; i++) {
            bool is_dup = false;
            for (int j = 0; j < unique_count; j++) {
                if (memcmp(unique_records[j].bssid, ap_records[i].bssid, 6) == 0) {
                    is_dup = true;
                    break;
                }
            }
            if (!is_dup) {
                unique_records[unique_count++] = ap_records[i];
            }
        }

        ESP_LOGI(TAG, "Unique count: %d (was %d)", unique_count, ap_count);

        // Копируем уникальные в сообщение
        scan_msg->ap_count = unique_count;
        memcpy(scan_msg->ap_records, unique_records, unique_count * sizeof(wifi_ap_record_t));

        // Отправляем сообщение
        ESP_LOGI(TAG, "Sending %zu bytes via project_event_handler (ap_count=%d)", msg_size, unique_count);
        project_event_send(APP_EVENT_WIFI_SCAN_DONE, scan_msg, msg_size);

        // Освобождаем временные буферы
        heap_caps_free(scan_msg); // ← ВАЖНО: message уже отправлено и скопировано project_event_handler'ом, можно free
        free(unique_records);
        free(ap_records);

        // Удаляем обработчик (освобождение)
        esp_event_handler_instance_t instance = (esp_event_handler_instance_t)arg;
        if (instance) {
            esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, instance);
        }

        // Уведомляем event group
        if (s_scan_done_event) {
            xEventGroupSetBits(s_scan_done_event, 0x01);
        }
    }
}

void wifi_scan_and_print(void)
{
    /*static bool wifi_initialized = false;
    if (!wifi_initialized) {
        ESP_LOGI(TAG, "Initializing Wi-Fi for scan...");
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
        wifi_initialized = true;
    }*/

    esp_event_handler_instance_t instance_scan_done = NULL;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, WIFI_EVENT_SCAN_DONE, event_handler, NULL, &instance_scan_done));

    wifi_scan_config_t scan_config = {0};
    scan_config.show_hidden = true;
    scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scan_config.channel = 0;
    scan_config.scan_time.active.min = 100;
    scan_config.scan_time.active.max = 300;

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, false));

    // Опционально: ждать завершения
    if (!s_scan_done_event) {
        s_scan_done_event = xEventGroupCreate();
    }
    xEventGroupWaitBits(s_scan_done_event, 0x01, pdTRUE, pdFALSE, pdMS_TO_TICKS(15000));
}