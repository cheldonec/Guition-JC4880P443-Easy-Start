#include "ya_cloud_speech_recognition_app.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "audio/audio_codec_engine.h"
#include "../sys_app_wifi_manager/wifi_manager_app.h"
#include "cloud_yandex.h"
#include "board_config.h"
//#include "storage/sd_card_io.h"

static const char *TAG = "APP_STT";

#define YANDEX_API_KEY  "..."
#define YANDEX_FOLDER_ID "..."

static void stt_task(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(3000));

    // 🟢 Ждём подключения (до 20 сек)
    ESP_LOGI(TAG, "Waiting for Wi-Fi...");
    for (int i = 0; i < 40; i++) { // 40 × 500 мс = 20 сек
        if (app_wifi_is_sta_connected()) {
            ESP_LOGI(TAG, "✅ Wi-Fi connected");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (!app_wifi_is_sta_connected()) {
        ESP_LOGE(TAG, "❌ Wi-Fi not connected after 20s → abort STT");
        vTaskDelete(NULL);
        return;
    }

    // ✅ ТЕСТ 1: маленький GET
    char http_test_buf[256] = {0};
    ESP_LOGI(TAG, "=== Starting HTTP GET test ===");
    esp_err_t test_err = cloud_test_http_get(http_test_buf, sizeof(http_test_buf));
    if (test_err == ESP_OK) {
        ESP_LOGI(TAG, "✅ HTTP GET test PASSED");
    } else {
        ESP_LOGE(TAG, "❌ HTTP GET test FAILED: %s", esp_err_to_name(test_err));
        // ... продолжить или выйти ...
    }

    // ✅ ТЕСТ 2: маленький POST (multipart)
    char http_post_buf[256] = {0};
    ESP_LOGI(TAG, "=== Starting HTTP POST test ===");
    test_err = cloud_test_http_post(http_post_buf, sizeof(http_post_buf));
    if (test_err == ESP_OK) {
        ESP_LOGI(TAG, "✅ HTTP POST test PASSED");
    } else {
        ESP_LOGE(TAG, "❌ HTTP POST test FAILED: %s", esp_err_to_name(test_err));
        // если падает — это точно SDIO/драйвер
        vTaskDelete(NULL);
        return;
    }


    ESP_LOGI(TAG, "Testing STT...");
    uint32_t size = 0;
    uint8_t *audio = bsp_audio_engine_record_to_mem(2, &size);
    if (!audio) {
        ESP_LOGE(TAG, "Record failed");
        vTaskDelete(NULL);
        return;
    }

    char text[256] = {0};
    esp_err_t err = cloud_stt_yandex(audio, size, text, sizeof(text), YANDEX_API_KEY, YANDEX_FOLDER_ID);
    heap_caps_free(audio);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "✅ '%s'", text);
    } else {
        ESP_LOGE(TAG, "❌ STT failed: %s", esp_err_to_name(err));
    }

    vTaskDelete(NULL);
}

esp_err_t app_stt_demo_init(void)
{
    ESP_LOGI(TAG, "STT demo init");
    return ESP_OK;
}

void app_stt_demo_start(void)
{
    xTaskCreate(stt_task, "app_stt_task", 32768, NULL, 5, NULL);
}

void app_stt_demo_deinit(void) {}