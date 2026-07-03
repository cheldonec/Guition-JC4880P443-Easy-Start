// main/applications/time_sync/time_sync_app.c
#include "time_sync_app.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif_sntp.h"
#include "nvs_flash.h"
#include "time.h"
#include "sys/time.h"
#include "esp_event.h"
#include <string.h>
#include <inttypes.h>  
#include "esp_wifi.h"
#include "../sys_app_wifi_manager/wifi_manager_app.h"
#include "project_event_handler_manager/project_event_handler_manager.h"

static const char *TAG = "APP_TIME_SYNC";

static const char *s_timezone = NULL;  // ← NULL, чтобы сначала загрузить из NVS

#define STORAGE_NAMESPACE "time_storage"
#define STORAGE_KEY_TIMESTAMP "timestamp"
#define STORAGE_KEY_LAST_WIFI_TIME "last_wifi_time"

// Глобальные переменные

static bool sntp_started = false;
static bool sntp_started_from_task = false;
static time_t s_last_saved_timestamp = 0; // Для time_keep_alive_task

// ✅ Установка часового пояса (вынесена отдельно для повторного вызова)
static void set_timezone(void)
{
    setenv("TZ", "MSK-3", 1);
    tzset();
    ESP_LOGI(TAG, "🌍 Часовой пояс установлен: MSK (UTC+3)");
}

// --- Сохранение TZ в NVS ---
static esp_err_t save_timezone_to_nvs(const char *tz)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("time", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_str(handle, "tz", tz);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

// --- Загрузка TZ из NVS ---
static esp_err_t load_timezone_from_nvs(char *tz_out, size_t len)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("time", NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    size_t out_len = len;
    err = nvs_get_str(handle, "tz", tz_out, &out_len);
    nvs_close(handle);
    return err;
}

// Вспомогательная функция: сохранение времени по ключу
static esp_err_t save_time_to_nvs_with_key(const char *key, time_t time_val)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_i64(nvs_handle, key, (int64_t)time_val);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to write time to NVS (key='%s'): %s", key, esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ NVS commit failed for key='%s': %s", key, esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "✅ Time saved to NVS (key='%s'): %" PRId64, key, (int64_t)time_val);
    }

    nvs_close(nvs_handle);
    return err;
}

// Сохранение времени (всегда timestamp + last_wifi_time при Wi-Fi)
static esp_err_t save_time_to_nvs(time_t time_val)
{
    ESP_LOGI(TAG, "💾 Attempting to save time to NVS...");
    esp_err_t err = save_time_to_nvs_with_key(STORAGE_KEY_TIMESTAMP, time_val);
    if (app_wifi_is_sta_connected()) {
        err = save_time_to_nvs_with_key(STORAGE_KEY_LAST_WIFI_TIME, time_val);
    }
    return err;
}

// Новая функция: сохранение только при изменении времени (и не чаще 1 часа)
static esp_err_t save_time_to_nvs_if_changed(time_t now, bool force)
{
    if (!force) {
        // Пропускаем, если не прошёл хотя бы 1 час с последнего сохранения
        if (s_last_saved_timestamp > 0 && (now - s_last_saved_timestamp) < 3600) {
            return ESP_OK;
        }
    }

    esp_err_t err = save_time_to_nvs(now);
    if (err == ESP_OK) {
        s_last_saved_timestamp = now;
        ESP_LOGI(TAG, "⏰ Timestamp updated: %" PRId64, (int64_t)now);
    }
    return err;
}

// Колбэк: вызывается при успешной синхронизации времени
static void time_sync_notification_cb(struct timeval *tv)
{
    struct tm timeinfo;
    char strftime_buf[64];
    time_t now = tv->tv_sec;
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);

    ESP_LOGI(TAG, "✅ Время синхронизировано: %s", strftime_buf);
    save_time_to_nvs_if_changed(now, true);

    // 🔹 Отправляем событие
    time_synced_sys_msg_t msg = {
        .timestamp = now,
        .from_sntp = true
    };
    project_event_send(APP_EVENT_TIME_SYNCED, &msg, sizeof(msg));
}

// Инициализация SNTP (только инициализация, без start)
static esp_err_t init_sntp(void)
{
    set_timezone();

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.sync_cb = time_sync_notification_cb;
    config.start = false;

    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ SNTP init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "📡 SNTP configured");
    return ESP_OK;
}

// Запуск синхронизации — вызывается из обработчика IP_EVENT или из задачи
static void start_sntp_and_wait_sync(bool from_task)
{
    
    if (sntp_started) {
        ESP_LOGI(TAG, "⚠️ SNTP already started — skipping duplicate request");
        return;
    }

    if (init_sntp() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init SNTP");
        return;
    }

    esp_netif_sntp_start();
    sntp_started = true;
    sntp_started_from_task = from_task;  // 🟢 ИСПРАВЛЕНО: обновляем флаг при первом запуске
    ESP_LOGI(TAG, "📡 SNTP started%s...", from_task ? " (from task)" : " (from IP_EVENT)");

    // 🟢 Ждём до 20 секунд синхронизации (40 × 500 мс)
    int retry = 0;
    const int retry_count = 40;
    while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(500)) != ESP_OK && ++retry < retry_count) {
        if (retry % 10 == 0) { // 🟢 Не спамить каждые 500 мс
            ESP_LOGI(TAG, "⏳ Waiting for sync... (%d/%d)", retry, retry_count);
        }
    }

    if (retry == retry_count) {
        ESP_LOGW(TAG, "⚠️ SNTP sync timeout — continuing anyway");
    } else {
        ESP_LOGI(TAG, "✅ Time synced via SNTP!");
    }
}

// Восстановление времени из NVS (пробует оба ключа)
static esp_err_t load_time_from_nvs(time_t *out_time)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "⚠️ NVS not found (err=%s), will sync from SNTP", esp_err_to_name(err));
        return err;
    }

    int64_t timestamp = 0;
    err = nvs_get_i64(nvs_handle, STORAGE_KEY_TIMESTAMP, &timestamp);
    if (err != ESP_OK) {
        err = nvs_get_i64(nvs_handle, STORAGE_KEY_LAST_WIFI_TIME, &timestamp);
    }

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "⚠️ No time in NVS (keys not found)");
        nvs_close(nvs_handle);
        return err;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ NVS read error: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    if (timestamp > 0) {
        set_timezone();

        struct timeval tv = {
            .tv_sec = (time_t)timestamp,
            .tv_usec = 0
        };
        if (settimeofday(&tv, NULL) == 0) {
            *out_time = (time_t)timestamp;
            struct tm timeinfo;
            localtime_r(out_time, &timeinfo);
            char strftime_buf[64];
            strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
            ESP_LOGI(TAG, "✅ Time restored from NVS: %s", strftime_buf);
            nvs_close(nvs_handle);
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "❌ settimeofday failed");
        }
    } else {
        ESP_LOGW(TAG, "⚠️ Invalid timestamp=%" PRId64 " in NVS", (int64_t)timestamp);
    }

    nvs_close(nvs_handle);
    return ESP_FAIL;
}

// Событие отключения Wi-Fi → сохраняем текущее время (только last_wifi_time)
static void wifi_disconnect_handler(void *arg, esp_event_base_t base,
                                    int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "⚠️ Wi-Fi disconnected — saving last_wifi_time...");
        time_t now = time(NULL);
        save_time_to_nvs_with_key(STORAGE_KEY_LAST_WIFI_TIME, now);
    }
}

// Событие получения IP → запускаем SNTP (если не запущена из задачи)
static void got_ip_handler(void *arg, esp_event_base_t base,
                           int32_t id, void *data)
{
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        // Если SNTP уже запущен из задачи — пропускаем
        if (sntp_started) {
            ESP_LOGI(TAG, "✅ SNTP already started — skipping IP_EVENT handler");
            return;
        }
        ESP_LOGI(TAG, "🌐 Got IP — starting SNTP sync...");
        start_sntp_and_wait_sync(false);
    }
}

// Основная задача синхронизации — адаптивный выбор: SNTP сразу или из NVS
static void time_sync_task(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(5000)); // ждём инициализации Wi-Fi и сопроцессора

    ESP_LOGI(TAG, "🚀 Starting time sync process...");

    // 1. Если Wi-Fi уже подключён и SNTP уже запущен из IP_EVENT — пропускаем
    if (sntp_started) {
        ESP_LOGI(TAG, "✅ SNTP already started by IP_EVENT — skipping");
        vTaskDelete(NULL);
        return;
    }

    // 1a. Если Wi-Fi уже подключён — пробуем SNTP сразу
    if (app_wifi_is_sta_connected()) {
        ESP_LOGI(TAG, "✅ Wi-Fi connected immediately — trying SNTP now");

        // 🆕 Ждём DNS до 3 секунд (часто не успевает обновиться сразу после Wi-Fi)
        ESP_LOGI(TAG, "🔄 Waiting for DNS resolution...");
        vTaskDelay(pdMS_TO_TICKS(3000));

        start_sntp_and_wait_sync(true);
        vTaskDelete(NULL);
        return;
    }

    // 2. Wi-Fi не подключён — читаем из NVS
    time_t restored_time = 0;
    if (load_time_from_nvs(&restored_time) == ESP_OK) {
        ESP_LOGI(TAG, "✅ Time restored from NVS — waiting for IP to sync via SNTP");
        vTaskDelete(NULL);
        return;
    }

    // 3. Нет времени и нет Wi-Fi — ждём IP_EVENT для синхронизации
    ESP_LOGI(TAG, "ℹ️ No valid time in NVS and no Wi-Fi — will wait for IP_EVENT to sync via SNTP");
    vTaskDelete(NULL);
}

// Фоновая задача: раз в час обновляет timestamp в NVS (если Wi-Fi подключён)
static void time_keep_alive_task(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(5000)); // ждём инициализации

    ESP_LOGI(TAG, "⏰ Starting time keep-alive task...");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(3600 * 1000)); // 1 час

        if (app_wifi_is_sta_connected()) {
            time_t now = time(NULL);
            if (now > 0) {
                save_time_to_nvs_if_changed(now, false); // false = только если прошёл час
            }
        }
    }
}

esp_err_t app_time_sync_set_timezone(const char *tz)
{
    esp_err_t err = save_timezone_to_nvs(tz);
    if (err != ESP_OK) return err;

    s_timezone = tz;
    setenv("TZ", tz, 1);
    tzset();
    return ESP_OK;
}

// Инициализация
esp_err_t app_time_sync_init(const char *timezone)
{
    ESP_LOGI(TAG, "Initializing...");

    // 🔹 Сначала пытаемся загрузить TZ из NVS
    char stored_tz[32] = {0};
    if (load_timezone_from_nvs(stored_tz, sizeof(stored_tz)) == ESP_OK) {
        s_timezone = stored_tz;
        ESP_LOGI(TAG, "✅ TZ loaded from NVS: %s", s_timezone);
    } else {
        // 🔹 Если не удалось — используем переданный timezone или TZ_DEFAULT
        if (!timezone) {
            timezone = TZ_DEFAULT;
        }
        s_timezone = timezone;
        ESP_LOGI(TAG, "⚠️ TZ not in NVS → using provided: %s", s_timezone);

        // 🔹 Сохраняем его в NVS (на всякий случай)
        esp_err_t err = save_timezone_to_nvs(s_timezone);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "⚠️ Failed to save TZ to NVS: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "✅ TZ saved to NVS: %s", s_timezone);
        }
    }

    // 🔹 Устанавливаем TZ
    setenv("TZ", s_timezone, 1);
    tzset();
    ESP_LOGI(TAG, "🌍 Часовой пояс установлен: %s", s_timezone);

    // ... остальной код (регистрация обработчиков) ...
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                               &got_ip_handler, NULL);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                               &wifi_disconnect_handler, NULL);

    ESP_LOGI(TAG, "Initialized OK");
    return ESP_OK;
}

// Запуск
void app_time_sync_start(void)
{
    xTaskCreate(time_sync_task, "app_time_sync_task", 4096, NULL, 5, NULL);
    xTaskCreate(time_keep_alive_task, "time_keep_alive", 4096, NULL, 4, NULL);
}

// Деинициализация
void app_time_sync_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing...");
    ESP_LOGI(TAG, "Deinitialized OK");
}