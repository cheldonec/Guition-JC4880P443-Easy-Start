// cloud_speech_recognition/cloud_yandex.c — Версия с esp_http_client, CRT bundle и cJSON (исправлено под ESP-IDF v5.x)
#include "cloud_yandex.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "opus.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>

// ✅ Подключаем cJSON
#include "cJSON.h"

static const char *TAG = "YANDEX_CLOUD";

static const char *BOUNDARY = "----WebKitFormBoundary7MA4YWxkTrZu0gW";

// ✅ Глобальные переменные для передачи body в event_handler
static uint8_t *g_stt_body = NULL;
static size_t g_stt_body_size = 0;
static size_t g_stt_pos = 0;

// ✅ Event handler для отправки POST-данных чанками (по 512 байт)
static esp_err_t stt_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            // Данные ответа обрабатываются в основном коде — здесь просто пропускаем
            break;

        case HTTP_EVENT_ON_HEADER:
            // Обычно не используется, но можно добавить логирование заголовков
            break;

        case HTTP_EVENT_ON_FINISH:
            // Запрос завершён — можно логировать
            break;

        default:
            break;
    }
    return ESP_OK;
}

static esp_err_t send_stt_request(const uint8_t *opus_data, size_t opus_size,
                                  const char *api_key, char *result, size_t result_size,
                                  const char *folder_id)
{
    if (!opus_data || opus_size == 0 || !api_key || !folder_id || !result || result_size == 0) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    // ✅ Подсчёт размера multipart body
    size_t body_size = 0;
    body_size += strlen("--") + strlen(BOUNDARY) + strlen("\r\n");
    body_size += strlen("Content-Disposition: form-data; name=\"topic\"\r\n\r\ngeneral\r\n");
    body_size += strlen("--") + strlen(BOUNDARY) + strlen("\r\n");
    body_size += strlen("Content-Disposition: form-data; name=\"folderId\"\r\n\r\n") + strlen(folder_id) + strlen("\r\n");
    body_size += strlen("--") + strlen(BOUNDARY) + strlen("\r\n");
    body_size += strlen("Content-Disposition: form-data; name=\"audio\"; filename=\"audio.ogg\"\r\n");
    body_size += strlen("Content-Type: audio/ogg\r\n\r\n");
    body_size += opus_size;
    body_size += strlen("\r\n");
    body_size += strlen("--") + strlen(BOUNDARY) + strlen("--\r\n");

    // ✅ Алоцируем буфер body
    uint8_t *body = heap_caps_malloc(body_size, MALLOC_CAP_INTERNAL);
    if (!body) {
        ESP_LOGE(TAG, "Failed to alloc %zu bytes for multipart body", body_size);
        return ESP_FAIL;
    }

    size_t pos = 0;

    // 1. topic=general
    pos += sprintf((char*)body + pos, "--%s\r\n", BOUNDARY);
    pos += sprintf((char*)body + pos, "Content-Disposition: form-data; name=\"topic\"\r\n\r\ngeneral\r\n");

    // 2. folderId=...
    pos += sprintf((char*)body + pos, "--%s\r\n", BOUNDARY);
    pos += sprintf((char*)body + pos, "Content-Disposition: form-data; name=\"folderId\"\r\n\r\n%s\r\n", folder_id);

    // 3. audio=@audio.ogg (без OPUSHEAD! — Yandex сам определит формат)
    pos += sprintf((char*)body + pos, "--%s\r\n", BOUNDARY);
    pos += sprintf((char*)body + pos, "Content-Disposition: form-data; name=\"audio\"; filename=\"audio.ogg\"\r\n");
    pos += sprintf((char*)body + pos, "Content-Type: audio/ogg\r\n\r\n");
    memcpy(body + pos, opus_data, opus_size);
    pos += opus_size;

    pos += sprintf((char*)body + pos, "\r\n--%s--\r\n", BOUNDARY);

    // ✅ Настройка HTTP-клиента
    esp_http_client_config_t config = {
        .url = "https://stt.api.cloud.yandex.net/speech/v1/stt:recognize",
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = stt_event_handler,
        .buffer_size_tx = 512, // критично! уменьшает размер буфера transmit
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        heap_caps_free(body);
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    // ✅ Set headers
    char content_type[128];
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", BOUNDARY);
    esp_http_client_set_header(client, "Content-Type", content_type);

    // ✅ Формируем полный заголовок Authorization
    char auth_header[64];
    const char *prefix = (api_key[0] == 'y' && api_key[1] == '2' && api_key[2] == '.') 
                         ? "Bearer " 
                         : "Api-Key ";
    snprintf(auth_header, sizeof(auth_header), "%s%s", prefix, api_key);
    esp_http_client_set_header(client, "Authorization", auth_header);

    // ✅ Глобальные переменные для event_handler (если понадобятся)
    g_stt_body = body;
    g_stt_body_size = pos;
    g_stt_pos = 0;

    esp_err_t err = esp_http_client_set_post_field(client, (const char *)body, pos);
    heap_caps_free(body);  // body копируется внутри — можно освободить

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set POST field: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    // Теперь perform() сам отправит тело
    err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGE(TAG, "HTTP status = %d", status);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    // ✅ Читаем JSON-ответ
    size_t len = esp_http_client_read(client, result, result_size);
    if (len == 0 || len == result_size) {
        ESP_LOGE(TAG, "Response too large or empty (%zu)", len);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    result[len] = '\0';

    ESP_LOGV(TAG, "STT raw response: %s", result);

    esp_http_client_cleanup(client);

    // ✅ Парсинг через cJSON
    cJSON *json = cJSON_Parse(result);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON: %s", result);
        return ESP_FAIL;
    }

    cJSON *res_obj = cJSON_GetObjectItem(json, "result");
    if (!res_obj || !cJSON_IsString(res_obj)) {
        ESP_LOGE(TAG, "Missing/invalid 'result' field");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    size_t res_len = strlen(res_obj->valuestring);
    if (res_len >= result_size) res_len = result_size - 1;
    memcpy(result, res_obj->valuestring, res_len);
    result[res_len] = '\0';

    cJSON_Delete(json);
    return ESP_OK;
    
}

esp_err_t cloud_stt_yandex(const uint8_t *audio_data, uint32_t audio_size,
                           char *text, size_t text_size,
                           const char *api_key, const char *folder_id)
{
    if (!audio_data || audio_size == 0 || !text || !api_key || !folder_id) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Free DRAM before STT: %d bytes", esp_get_free_heap_size());

    OpusEncoder *encoder = opus_encoder_create(16000, 1, OPUS_APPLICATION_VOIP, NULL);
    if (!encoder) {
        ESP_LOGE(TAG, "Failed to create OPUS encoder");
        return ESP_FAIL;
    }
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(5));

    // ✅ НЕ добавляем OPUSHEAD — только сырые OPUS-данные
    size_t opus_size = audio_size * 2 / 3 + 20;
    uint8_t *opus_data = heap_caps_malloc(opus_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!opus_data) {
        ESP_LOGE(TAG, "Failed to alloc %zu bytes in DRAM", opus_size);
        opus_encoder_destroy(encoder);
        return ESP_FAIL;
    }

    int total_size = 0;

    const int frame_size = 960; // 60 мс при 16 кГц
    const int16_t *pcm = (const int16_t *)audio_data;
    uint32_t pcm_pos = 0;
    uint8_t *opus_frame = heap_caps_malloc(1500, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!opus_frame) {
        heap_caps_free(opus_data);
        opus_encoder_destroy(encoder);
        return ESP_FAIL;
    }

    while (pcm_pos + frame_size <= audio_size / 2) {
        int16_t frame[frame_size];
        memcpy(frame, pcm + pcm_pos, sizeof(frame));

        int len = opus_encode(encoder, frame, frame_size, opus_frame, 1500);
        if (len > 0 && total_size + len <= opus_size) {
            memcpy(opus_data + total_size, opus_frame, len);
            total_size += len;
        }
        pcm_pos += frame_size;
    }

    heap_caps_free(opus_frame);
    opus_encoder_destroy(encoder);

    ESP_LOGI(TAG, "OPUS encoded: %d bytes (DRAM)", total_size);
    ESP_LOGI(TAG, "Free DRAM before STT HTTP: %d bytes", esp_get_free_heap_size());

    esp_err_t ret = send_stt_request(opus_data, total_size, api_key, text, text_size, folder_id);

    heap_caps_free(opus_data);
    return ret;
}

// 🎙️ TTS: синтез речи (Yandex TTS v1)
esp_err_t cloud_tts_yandex(
    const char *text,
    uint8_t **wav_buffer_out,
    uint32_t *wav_size_out,
    const char *api_key,
    const char *folder_id,
    const char *voice,
    const char *emotion)
{
    if (!text || !wav_buffer_out || !api_key || !voice) {
        return ESP_ERR_INVALID_ARG;
    }

    *wav_buffer_out = NULL;
    *wav_size_out = 0;

    ESP_LOGI(TAG, "Synthesizing TTS: '%s' (%s, %s)", text, voice, emotion ? emotion : "neutral");
    ESP_LOGI(TAG, "Free DRAM before TTS: %d bytes", esp_get_free_heap_size());

    // ✅ Безопасный формат JSON
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "text", text);
    cJSON_AddStringToObject(req, "lang", "ru-RU");
    cJSON_AddStringToObject(req, "voice", voice);
    if (emotion) {
        cJSON_AddStringToObject(req, "emotion", emotion);
    }
    char *request_body = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);

    if (!request_body) {
        ESP_LOGE(TAG, "Failed to build JSON");
        return ESP_FAIL;
    }

    esp_http_client_config_t config = {
        .url = "https://tts.api.cloud.yandex.net/speech/v1/tts:synthesize",
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(request_body);
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    // ✅ Authorization: "Bearer <token>" или "Api-Key <key>"
    char auth_header[64];
    const char *prefix = (api_key[0] == 'y' && api_key[1] == '2' && api_key[2] == '.') 
                         ? "Bearer " 
                         : "Api-Key ";
    snprintf(auth_header, sizeof(auth_header), "%s%s", prefix, api_key);
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    esp_err_t err = esp_http_client_set_post_field(client, request_body, strlen(request_body));
    free(request_body); // освобождаем сразу после set_post_field

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set POST field");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGE(TAG, "TTS HTTP status = %d", status);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    // ✅ Читаем WAV как бинарные данные
    size_t total_read = 0, buf_size = 2048;
    *wav_buffer_out = heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!(*wav_buffer_out)) {
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    while (1) {
        char buf[1024];
        int n = esp_http_client_read(client, buf, sizeof(buf));
        if (n <= 0) break;
        if (total_read + n > buf_size) {
            buf_size *= 2;
            uint8_t *new_buf = heap_caps_realloc(*wav_buffer_out, buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (!new_buf) {
                heap_caps_free(*wav_buffer_out);
                *wav_buffer_out = NULL;
                esp_http_client_cleanup(client);
                return ESP_FAIL;
            }
            *wav_buffer_out = new_buf;
        }
        memcpy(*wav_buffer_out + total_read, buf, n);
        total_read += n;
    }

    esp_http_client_cleanup(client);

    if (total_read == 0) {
        heap_caps_free(*wav_buffer_out);
        *wav_buffer_out = NULL;
        return ESP_FAIL;
    }

    *wav_size_out = total_read;
    ESP_LOGI(TAG, "TTS ready: %lu bytes (DRAM)", total_read);

    return ESP_OK;
}

// 🔍 Тестовая функция: проверка минимального GET-запроса по Wi-Fi/SDIO
esp_err_t cloud_test_http_get(char *result, size_t result_size)
{
    if (!result || result_size == 0) {
        ESP_LOGE(TAG, "Invalid result buffer");
        return ESP_ERR_INVALID_ARG;
    }

    esp_http_client_config_t config = {
        .url = "http://example.com",
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
        .crt_bundle_attach = NULL,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client (test)");
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "User-Agent", "ESP32-P4-STT-Test");
    esp_http_client_set_header(client, "Connection", "close");

    ESP_LOGI(TAG, "Test: Sending GET to http://example.com...");

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Test HTTP GET failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int status = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "✅ Test HTTP status: %d", status);

    if (status != 200) {
        ESP_LOGE(TAG, "❌ Expected 200, got %d", status);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    // ✅ Читаем ответ В ЦИКЛЕ (как в cloud_tts_yandex)
    size_t total_read = 0;
    int buf_size = 512;
    char *buf = heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to alloc test buffer");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    while (1) {
        int n = esp_http_client_read(client, buf + total_read, buf_size - total_read);
        if (n <= 0) break;
        total_read += n;

        if (total_read == buf_size) {
            buf_size *= 2;
            char *new_buf = heap_caps_realloc(buf, buf_size, MALLOC_CAP_INTERNAL);
            if (!new_buf) {
                ESP_LOGE(TAG, "Failed to realloc buffer");
                heap_caps_free(buf);
                esp_http_client_cleanup(client);
                return ESP_FAIL;
            }
            buf = new_buf;
        }
    }

    // копируем в result
    size_t copy_len = MIN(total_read, result_size - 1);
    memcpy(result, buf, copy_len);
    result[copy_len] = '\0';

    heap_caps_free(buf);
    esp_http_client_cleanup(client);

    ESP_LOGI(TAG, "✅ Test: got %zu bytes", copy_len);
    ESP_LOGD(TAG, "Test response (first 200 chars): %.200s", result);

    return ESP_OK;
}

// 🔍 Тест: отправка небольшого POST-запроса (multipart)
// 🔍 Тест: отправка небольшого POST-запроса (multipart) — исправлено
esp_err_t cloud_test_http_post(char *result, size_t result_size)
{
    if (!result || result_size == 0) {
        ESP_LOGE(TAG, "Invalid result buffer");
        return ESP_ERR_INVALID_ARG;
    }

    const char *boundary = "----ESP32P4TestBoundary";
    const char *test_data = "Hello from ESP32-P4 STT test";
    size_t test_data_len = strlen(test_data);

    size_t body_len = 0;
    body_len += strlen("--") + strlen(boundary) + strlen("\r\n");
    body_len += strlen("Content-Disposition: form-data; name=\"test\"\r\n\r\n");
    body_len += test_data_len;
    body_len += strlen("\r\n");
    body_len += strlen("--") + strlen(boundary) + strlen("--\r\n");

    uint8_t *body = heap_caps_malloc(body_len, MALLOC_CAP_INTERNAL);
    if (!body) {
        ESP_LOGE(TAG, "Failed to alloc body");
        return ESP_FAIL;
    }

    size_t pos = 0;
    pos += sprintf((char*)body + pos, "--%s\r\n", boundary);
    pos += sprintf((char*)body + pos, "Content-Disposition: form-data; name=\"test\"\r\n\r\n");
    memcpy(body + pos, test_data, test_data_len);
    pos += test_data_len;
    pos += sprintf((char*)body + pos, "\r\n--%s--\r\n", boundary);

    ESP_LOGI(TAG, "Test POST: %zu bytes body, %zu chars data", body_len, test_data_len);

    esp_http_client_config_t config = {
        .url = "https://stt.api.cloud.yandex.net/speech/v1/stt:recognize",
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size_tx = 256,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        heap_caps_free(body);
        ESP_LOGE(TAG, "Failed to init HTTP client (test POST)");
        return ESP_FAIL;
    }

    // Устанавливаем заголовки ДО perform()
    char ct[64];
    snprintf(ct, sizeof(ct), "multipart/form-data; boundary=%s", boundary);
    esp_http_client_set_header(client, "Content-Type", ct);
    esp_http_client_set_header(client, "Authorization", "Bearer dummy_api_key");

    esp_err_t err = esp_http_client_set_post_field(client, (const char *)body, pos);
    heap_caps_free(body);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set POST field: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Test POST: sending...");

    err = esp_http_client_perform(client);
    
    // ← ВАЖНО: не проверять err == ESP_OK, потому что 401 — не сетевая ошибка!
    int status = esp_http_client_get_status_code(client);

    if (err != ESP_OK && err != ESP_ERR_HTTP_CONNECT) {
        ESP_LOGE(TAG, "❌ Test HTTP POST failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "✅ Test POST HTTP status: %d", status);

    // Ожидаем 401 (неавторизован) — это OK!
    if (status == 401) {
        ESP_LOGI(TAG, "✅ POST accepted (expected 401 auth error)");
    } else {
        ESP_LOGE(TAG, "❌ Expected 401, got %d", status);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    // Читаем ответ
    size_t len = esp_http_client_read(client, result, result_size);
    if (len > 0) {
        result[len] = '\0';
        ESP_LOGI(TAG, "Test POST response (%zu bytes): %.200s", len, result);
    }

    esp_http_client_cleanup(client);
    return ESP_OK;
}