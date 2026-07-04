// main/applications/sys_app_sd_card_info/sd_card_info_app.c

#include "sd_card_info_app.h"
#include "project_event_handler_manager/project_event_handler_manager.h"
#include "esp_log.h"
#include "dirent.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "APP_SD_INFO";

// === Структура для хранения списка файлов ===
typedef struct {
    uint16_t count;
    char names[32][64];  // максимум 32 файла по 63 байта каждый
} sd_file_names_t;

static void list_sd_files_task(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(500));

    DIR *dir = opendir("/sdcard");
    if (!dir) {
        ESP_LOGE(TAG, "❌ Failed to open /sdcard!");
        return;
    }

    sd_file_names_t *file_list = heap_caps_malloc(sizeof(sd_file_names_t), MALLOC_CAP_DEFAULT);
    if (!file_list) {
        ESP_LOGE(TAG, "❌ Failed to allocate file_list!");
        closedir(dir);
        return;
    }
    
    file_list->count = 0;

    struct dirent *de;
    while ((de = readdir(dir)) != NULL && file_list->count < 32) {
        if (de->d_type == DT_REG) {
            strncpy(file_list->names[file_list->count], de->d_name, 63);
            file_list->names[file_list->count][63] = '\0';
            file_list->count++;
            ESP_LOGI(TAG, "📄 Found: %s", file_list->names[file_list->count - 1]);
        }
    }
    closedir(dir);

    // === Отправляем событие со списком файлов ===
    size_t msg_size = sizeof(sd_files_list_sys_msg_t) + file_list->count * 64;
    sd_files_list_sys_msg_t *msg = heap_caps_malloc(msg_size, MALLOC_CAP_DEFAULT);
    if (msg) {
        msg->file_count = file_list->count;
        for (uint16_t i = 0; i < file_list->count; i++) {
            strcpy(&msg->filenames[i][0], file_list->names[i]);
        }
        
        esp_err_t ret = project_event_send(APP_EVENT_SD_FILES_LIST, msg, msg_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to send SD_FILES_LIST event: %d", ret);
            heap_caps_free(msg);
        } else {
            ESP_LOGI(TAG, "✅ Sent SD_FILES_LIST event: %u files", file_list->count);
        }
    } else {
        ESP_LOGE(TAG, "❌ Failed to allocate msg for SD_FILES_LIST");
    }

    heap_caps_free(file_list);
    vTaskDelete(NULL);
}

esp_err_t app_sd_card_info_init(void)
{
    ESP_LOGI(TAG, "SD card info init");
    return ESP_OK;
}

void app_sd_card_info_start(void)
{
    xTaskCreate(list_sd_files_task, "app_sd_info_task", 4096, NULL, 5, NULL);
}

void app_sd_card_info_deinit(void) {}