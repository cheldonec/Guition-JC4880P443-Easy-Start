#include "sd_card_info_app.h"
//#include "storage/sd_card_io.h" // для list_sd_files(), если нужен
#include "esp_log.h"
#include "dirent.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "APP_SD_INFO";

static void list_sd_files_task(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(500)); // дать SD время инициализироваться

    DIR *dir = opendir("/sdcard");
    if (dir) {
        struct dirent *de;
        int file_count = 0;
        while ((de = readdir(dir)) != NULL) {
            if (de->d_type == DT_REG) {
                ESP_LOGI(TAG, "📄 %s", de->d_name);
                file_count++;
            }
        }
        closedir(dir);
        ESP_LOGI(TAG, "📁 Total files on SD: %d", file_count);
    } else {
        ESP_LOGE(TAG, "❌ Failed to open /sdcard!");
    }

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