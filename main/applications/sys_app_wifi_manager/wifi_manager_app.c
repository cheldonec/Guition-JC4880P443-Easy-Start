// main/applications/wifi_manager/wifi_manager_app.c
#include "wifi_manager_app.h"
#include "wifi_manager_scan_and_print.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "project_event_handler_manager/project_event_handler_manager.h"
#include "board_init.h"

static const char *TAG = "APP_WIFI";

// 🔐 Глобальный мьютекс для защиты NVS
static SemaphoreHandle_t s_nvs_mutex = NULL;

// --- Инициализация мьютекса (вызывается из main.c до использования NVS) ---
static esp_err_t wifi_init_nvs_mutex(void)
{
    if (s_nvs_mutex) {
        ESP_LOGW(TAG, "⚠️ s_nvs_mutex already initialized");
        return ESP_OK;
    }

    s_nvs_mutex = xSemaphoreCreateMutex();
    if (!s_nvs_mutex) {
        ESP_LOGE(TAG, "❌ Не удалось создать s_nvs_mutex");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "✅ s_nvs_mutex created");
    return ESP_OK;
}

esp_err_t app_wifi_init(void)
{
    ESP_LOGI(TAG, "Initializing Wi-Fi NVS mutex..");
    esp_err_t err = wifi_init_nvs_mutex();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to init s_nvs_mutex: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Initializing Wi-Fi...");

    project_event_register_handler(APP_EVENT_REQUEST_WIFI_SCAN, (project_event_cb_t)wifi_scan_and_print);

    // Регистрируем только обработчики для сканирования (если нужно)
    // асинхронные обработчики подключения — в wifi_start_sta.c

    return ESP_OK;
}

void wifi_scan_networks(void)
{
    project_event_send(APP_EVENT_REQUEST_WIFI_SCAN, NULL, 0);
}



// --- Сохранение учётных данных в NVS ---
esp_err_t wifi_set_STA_credentials(uint8_t bucket_0_10, const char *ssid, const char *password)
{
    if (!s_nvs_mutex) {
        ESP_LOGE(TAG, "❌ s_nvs_mutex not initialized. Call wifi_init_nvs_mutex() first!");
        return ESP_ERR_INVALID_STATE;
    }

    if (bucket_0_10 > 10) {
        ESP_LOGE(TAG, "❌ Invalid bucket: %u (must be 0..10)", bucket_0_10);
        return ESP_ERR_INVALID_ARG;
    }

    char key_ssid[32], key_pass[32];
    snprintf(key_ssid, sizeof(key_ssid), "sta_ssid_%u", bucket_0_10);
    snprintf(key_pass, sizeof(key_pass), "sta_pass_%u", bucket_0_10);

    if (xSemaphoreTake(s_nvs_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "❌Failed to take s_nvs_mutex");
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("wifi", NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        xSemaphoreGive(s_nvs_mutex);
        return ret;
    }

    ret = nvs_set_str(handle, key_ssid, ssid);
    if (ret == ESP_OK) {
        ret = nvs_set_str(handle, key_pass, password ? password : "");
    }
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }

    nvs_close(handle);
    xSemaphoreGive(s_nvs_mutex);
    return ret;
}

// --- Загрузка учётных данных из NVS ---
esp_err_t wifi_load_STA_credentials(uint8_t bucket_0_10, char *ssid_out, size_t ssid_len, char *pass_out, size_t pass_len)
{
    if (!s_nvs_mutex) {
        ESP_LOGE(TAG, "❌ s_nvs_mutex not initialized. Call wifi_init_nvs_mutex() first!");
        return ESP_ERR_INVALID_STATE;
    }

    if (bucket_0_10 > 10) {
        ESP_LOGE(TAG, "❌ Invalid bucket: %u (must be 0..10)", bucket_0_10);
        return ESP_ERR_INVALID_ARG;
    }

    char key_ssid[32], key_pass[32];
    snprintf(key_ssid, sizeof(key_ssid), "sta_ssid_%u", bucket_0_10);
    snprintf(key_pass, sizeof(key_pass), "sta_pass_%u", bucket_0_10);

    if (xSemaphoreTake(s_nvs_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "❌Failed to take s_nvs_mutex");
        return ESP_ERR_TIMEOUT;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        xSemaphoreGive(s_nvs_mutex);
        return err;
    }

    size_t len = ssid_len;
    err = nvs_get_str(handle, key_ssid, ssid_out, &len);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "[NVS] Loaded bucket=%u: SSID='%s' (len=%zu)", bucket_0_10, ssid_out, len);
        len = pass_len;
        err = nvs_get_str(handle, key_pass, pass_out, &len);
    }

    nvs_close(handle);
    xSemaphoreGive(s_nvs_mutex);
    return err;
}

bool app_wifi_is_sta_connected(void)
{
    return s_wifi_connected;
}


