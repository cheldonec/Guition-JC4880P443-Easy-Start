// main/applications/wifi_manager/wifi_start_sta.c
#include "wifi_manager_app.h"
#include "esp_log.h"
#include "project_event_handler_manager/project_event_handler_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "board_init.h"

static const char* TAG = "wifi_start_sta";
bool s_wifi_connected = false;

// Глобальные переменные для текущего подключения
static char last_ssid[32] = {0};
static char last_pass[64] = {0};

// Состояние очереди подключения (глобальное)
static int g_wifi_connection_stage = 0; // 0 = none, 1 = bucket 0, 2 = 1..10, 3 = reserv, 4 = loop-reset
static int g_wifi_retry_count = 0;
static const int g_wifi_retry_max = 5;

// ← ДОБАВЛЕНО: цикличность
static int g_wifi_connection_loop_count = 0;
static const int g_wifi_max_loops = 3;

// 🚩 ДОБАВЛЕНО: флаги
static bool g_has_tried_all_buckets = false;
static bool g_first_attempt = true; // ← НОВОЕ: только при первом запуске

// 🔹 ДОБАВЛЕНО: номер последнего слота в stage=2
static int g_wifi_last_bucket_index = 0; // 0 = не запущен, 1..10 = последний слот

// --- Обработчик событий (копия из wifi_manager.c) ---
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA started");
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t*)event_data;
        ESP_LOGW(TAG, "WiFi STA disconnected: reason=%d, ssid=%s", disconn->reason, last_ssid);

        // 🔹 Отправляем событие DISCONNECTED
        project_event_send(APP_EVENT_WIFI_DISCONNECTED, NULL, 0);

        // 🔹 Уменьшаем retry для reason=205 (ASSOC_LEAVE) — это не фатальная ошибка
        if (disconn->reason == 205) {
            ESP_LOGW(TAG, "⚠️ reason=205 (ASSOC_LEAVE) → skip incrementing retry");
            // не увеличиваем g_wifi_retry_count
        } else {
            g_wifi_retry_count++;
        }

        ESP_LOGW(TAG, "Retry %d/%d", g_wifi_retry_count, g_wifi_retry_max);

        if (g_wifi_retry_count <= g_wifi_retry_max && strlen(last_ssid) > 0) {
            // 🔹 Добавляем задержку перед повтором
            ESP_LOGI(TAG, " delaying 1500 ms before reconnect...");
            vTaskDelay(pdMS_TO_TICKS(1500));
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "❌ Max retries (%d/%d) exceeded for SSID=%s", 
                     g_wifi_retry_count, g_wifi_retry_max, last_ssid);
            // Перейти к следующему источнику
            g_wifi_connection_stage++;
            g_wifi_retry_count = 0;
            // Вызвать app_wifi_start_sta() рекурсивно — передаём старый netif
            app_wifi_start_sta(sta_netif, NULL, NULL); // stage автоматически перейдёт дальше
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "✅ Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        // 🔹 Подготовка сообщения
        wifi_connected_sys_msg_t msg = {
            .ip = event->ip_info.ip,
            .ssid = {0}
        };
        strncpy(msg.ssid, last_ssid, sizeof(msg.ssid) - 1);

        // 🔹 Отправляем событие CONNECTED
        project_event_send(APP_EVENT_WIFI_CONNECTED, &msg, sizeof(msg));

        // 🔹 Устанавливаем флаг подключения
        s_wifi_connected = true;

        if (strlen(last_ssid) > 0) {
            esp_err_t err = wifi_set_STA_credentials(0, last_ssid, last_pass);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "✅ Credentials saved to bucket=0: SSID=%s", last_ssid);
            } else {
                ESP_LOGE(TAG, "❌ Failed to save credentials: %s", esp_err_to_name(err));
            }
        }

        // 🔁 СБРОС цикличности при успешном подключении
        g_wifi_retry_count = 0;
        g_wifi_connection_stage = 0;
        g_has_tried_all_buckets = false;
        g_wifi_connection_loop_count = 0;
        g_first_attempt = true;
        g_wifi_last_bucket_index = 0; // ← сброс последнего слота
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // 🔹 Сбрасываем флаг при отключении
        s_wifi_connected = false;
    }
}

// Инициализация обработчиков
static void ensure_event_handlers(void)
{
    static bool handlers_registered = false;
    if (!handlers_registered) {
        esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_START,
                                            event_handler, NULL, NULL);
        esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                            event_handler, NULL, NULL);
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                            event_handler, NULL, NULL);
       
        handlers_registered = true;
    }
}

// Подключение с сохранением учётных данных
static esp_err_t attempt_connection(const char *ssid, const char *pass)
{
    ESP_LOGI(TAG, "attempt_connection(): SSID=%s, PASS=***", ssid);

    esp_err_t ret = ESP_OK;
    wifi_config_t cfg = {0};

    strncpy(last_ssid, ssid, sizeof(last_ssid) - 1);
    last_ssid[sizeof(last_ssid) - 1] = '\0';

    if (pass && strlen(pass) > 0) {
        strncpy(last_pass, pass, sizeof(last_pass) - 1);
        last_pass[sizeof(last_pass) - 1] = '\0';
    } else {
        last_pass[0] = '\0';
    }

    strncpy((char*)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid) - 1);
    cfg.sta.ssid[sizeof(cfg.sta.ssid) - 1] = '\0';

    if (pass && strlen(pass) > 0) {
        strncpy((char*)cfg.sta.password, pass, sizeof(cfg.sta.password) - 1);
        cfg.sta.password[sizeof(cfg.sta.password) - 1] = '\0';
    }

    ESP_LOGI(TAG, "Calling esp_wifi_set_config()...");
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    ESP_LOGI(TAG, "Calling esp_wifi_start()...");
    esp_wifi_start();
    ESP_LOGI(TAG, "Calling esp_wifi_connect()...");
    esp_wifi_connect();

    g_wifi_retry_count = 0;
    ESP_LOGI(TAG, "attempt_connection() returned ESP_OK");
    return ESP_OK;
}

// ✅ Асинхронное подключение (НЕБЛОКИРУЮЩЕЕ)
// В параметрах резервные SSID и пароль , если сохранённые сети не подключаются
esp_err_t app_wifi_start_sta(esp_netif_t *sta_netif, const char *reserv_ssid, const char *reserv_pass)
{
    ESP_LOGI(TAG, "app_wifi_start_sta() called: stage=%d, sta_netif=%p, reserv_ssid=%s, loop=%d, tried_all=%s, first=%s, last_bucket=%d",
             g_wifi_connection_stage, sta_netif, reserv_ssid ? reserv_ssid : "(null)",
             g_wifi_connection_loop_count, g_has_tried_all_buckets ? "true" : "false",
             g_first_attempt ? "true" : "false", g_wifi_last_bucket_index);

    // 1. Инициализация обработчиков событий
    ensure_event_handlers();

    if (g_wifi_connection_stage > 4) {
        ESP_LOGE(TAG, "❌ stage > 4 → recursive overflow prevention");
        return ESP_FAIL;
    }

    // 🔧 ИСПРАВЛЕНИЕ: ВСЕГДА используем sta_netif, если он передан (не NULL)
    static bool netif_created = false;
    if (!sta_netif && !netif_created) {
        ESP_LOGI(TAG, "Creating default wifi sta netif...");
        sta_netif = esp_netif_create_default_wifi_sta();
        netif_created = true;
        if (!sta_netif) {
            ESP_LOGE(TAG, "❌ Failed to create default wifi sta netif");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "✅ Default wifi sta netif created");
    }

    // 2. Получение текущего режима
    wifi_mode_t mode;
    esp_err_t ret = esp_wifi_get_mode(&mode);
    ESP_LOGI(TAG, "esp_wifi_get_mode() returned %d", ret); // 12289
    //W (4014) rpc_rsp: Hosted RPC_Resp [0x203], uid [2], resp code [12289]  normal (WIFI not init yet)
    if ((ret == ESP_OK) && (mode != WIFI_MODE_STA)) {
        ESP_LOGI(TAG, "mode != WIFI_MODE_STA, trying to set WIFI_MODE_STA");
        ret = esp_wifi_set_mode(WIFI_MODE_STA);
        if (ret != ESP_OK) return ret;
        ESP_LOGI(TAG, "SET WIFI_MODE_STA OK");
    } else if (ret == ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGI(TAG, "ESP_ERR_WIFI_NOT_INIT → init Wi-Fi");

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ret = esp_wifi_init(&cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to init Wi-Fi");
            return ret;
        }

        if (!sta_netif) {
            sta_netif = esp_netif_create_default_wifi_sta();
            if (!sta_netif) {
                ESP_LOGE(TAG, "❌ Failed to create default wifi sta netif");
                return ESP_FAIL;
            }
        }

        ret = esp_wifi_set_mode(WIFI_MODE_STA);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to set Wi-Fi mode");
            return ret;
        }
        ESP_LOGI(TAG, "Wi-Fi driver initialized (mode=STA)");
    } else if (ret == ESP_OK && !sta_netif) {
        sta_netif = esp_netif_create_default_wifi_sta();
        if (!sta_netif) {
            ESP_LOGE(TAG, "❌ Failed to create default wifi sta netif");
            return ESP_FAIL;
        }
    }

    // 🔁 Логика подключения по очереди
    if (g_wifi_connection_stage == 0) { // старт
        g_wifi_connection_stage = 1;
        g_wifi_retry_count = 0;
        g_has_tried_all_buckets = false;
        g_first_attempt = true;
        g_wifi_last_bucket_index = 0; // ← сброс индекса
    }

    char ssid[32] = {0};
    char pass[64] = {0};

    ESP_LOGI(TAG, "Switching to stage %d", g_wifi_connection_stage);

    switch (g_wifi_connection_stage) {
        case 1: {
            ESP_LOGI(TAG, "➡️ stage 1: trying NVS bucket=0");
            esp_err_t err = wifi_load_STA_credentials(0, ssid, sizeof(ssid), pass, sizeof(pass));
            if (err == ESP_OK && strlen(ssid) > 0) {
                ESP_LOGI(TAG, "✅ bucket=0 loaded: SSID=%s", ssid);
                g_wifi_retry_count = 0;
                ret = attempt_connection(ssid, pass);
                return ret;
            } else {
                ESP_LOGW(TAG, "❌ bucket=0 empty or err=%d", err);
                g_wifi_last_bucket_index = 0; // ← сброс перед переходом в stage=2
                g_wifi_connection_stage = 2;
                return app_wifi_start_sta(sta_netif, reserv_ssid, reserv_pass);
            }
        }

        case 2: {
            ESP_LOGI(TAG, "➡️ stage 2: trying NVS buckets 1..10");
            int start_bucket = g_wifi_last_bucket_index + 1; // начать с следующего
            if (start_bucket > 10) start_bucket = 1;

            for (int i = start_bucket; i <= 10; i++) {
                ESP_LOGI(TAG, "🔹 Trying bucket=%d...", i);
                esp_err_t err = wifi_load_STA_credentials(i, ssid, sizeof(ssid), pass, sizeof(pass));
                if (err != ESP_OK || strlen(ssid) == 0) {
                    ESP_LOGW(TAG, "❌ bucket=%d empty or err=%d", i, err);
                    continue;
                }

                ESP_LOGI(TAG, "✅ bucket=%d loaded: SSID=%s", i, ssid);
                g_wifi_retry_count = 0;
                ret = attempt_connection(ssid, pass);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "✅ attempt_connection() started for bucket=%d", i);
                    g_wifi_last_bucket_index = i; // ← запоминаем слот
                    return ret;
                }
                ESP_LOGW(TAG, "⚠️ attempt_connection() failed for bucket=%d, trying next...", i);
            }
            ESP_LOGW(TAG, "❌ no bucket 1..10 succeeded");
            if (g_wifi_last_bucket_index > 10) g_wifi_last_bucket_index = 0; // сброс
            //g_wifi_last_bucket_index = 0; // сброс
            g_wifi_connection_stage = 3;
            return app_wifi_start_sta(sta_netif, reserv_ssid, reserv_pass);
        }

        case 3: {
            // 🔁 Если это не первая попытка — идём в цикл (не в reserv)
            if (!g_first_attempt) {
                ESP_LOGW(TAG, "⚠️ not first attempt → fallback to loop");
                g_wifi_connection_stage = 4;
                return app_wifi_start_sta(sta_netif, reserv_ssid, reserv_pass);
            }
            if (!reserv_ssid) {
                ESP_LOGW(TAG, "⚠️ no reserv_ssid → fallback to loop");
                g_wifi_connection_stage = 4;
                return app_wifi_start_sta(sta_netif, reserv_ssid, reserv_pass);
            }
            ESP_LOGI(TAG, "➡️ stage 3: using reserv_ssid='%s'", reserv_ssid);
            strncpy(ssid, reserv_ssid, sizeof(ssid) - 1);
            ssid[sizeof(ssid) - 1] = '\0';
            if (reserv_pass && strlen(reserv_pass) > 0) {
                strncpy(pass, reserv_pass, sizeof(pass) - 1);
                pass[sizeof(pass) - 1] = '\0';
            } else {
                pass[0] = '\0';
            }
            g_wifi_retry_count = 0;
            ret = attempt_connection(ssid, pass);
            return ret;
        }

        case 4: {
            // 🔁 Новый этап: перезапуск цикла (всё по кругу)
            g_wifi_connection_loop_count++;
            ESP_LOGW(TAG, "🔄 Loop %d: restarting from bucket=0 (no success yet)", g_wifi_connection_loop_count);
            if (g_wifi_connection_loop_count >= g_wifi_max_loops) {
                ESP_LOGW(TAG, "⚠️ max loops (%d) reached — continuing anyway", g_wifi_max_loops);
            }

            g_first_attempt = false;
            g_has_tried_all_buckets = true;
            //g_wifi_last_bucket_index = 0;
            g_wifi_connection_stage = 1;
            g_wifi_retry_count = 0;
            return app_wifi_start_sta(sta_netif, reserv_ssid, reserv_pass);
        }

        default:
            ESP_LOGE(TAG, "❌ unexpected stage=%d", g_wifi_connection_stage);
            return ESP_FAIL;
    }
}