// main/applications/sys_app_audio_play_and_rec/audio_play.c
#include "audio_play.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "project_event_handler_manager/project_event_handler_manager.h"
#include "esp_log.h"
#include "esp_codec_dev_os.h"

static const char *TAG = "AUDIO_PLAY";

static esp_codec_dev_mutex_handle_t s_mutex = NULL;
// === Глобальный флаг остановки воспроизведения ===
static bool s_play_stop_requested = false;

void audio_play_init(void)
{
    if (!s_mutex) {
        s_mutex = esp_codec_dev_mutex_create();
        if (s_mutex) ESP_LOGI(TAG, "✅ Mutex created");
        else ESP_LOGE(TAG, "❌ Failed to create mutex");
    }
}

void audio_play_player_stop(void)
{
    s_play_stop_requested = true;
    ESP_LOGI(TAG, "🛑 Запрошена остановка воспроизведения");
}

// === Синхронная функция воспроизведения из буфера ===

static esp_err_t audio_play_from_mem(const uint8_t *buffer, uint32_t size, uint32_t *bytes_played_ptr)
{
    
    s_play_stop_requested = false;
    if (!audio_dev_es8311.play_dev) {
        ESP_LOGE(TAG, "❌ Play device not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!buffer || size == 0) {
        ESP_LOGE(TAG, "❌ Invalid buffer or size");
        return ESP_ERR_INVALID_ARG;
    }

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 16000, // ← из board_config.h AUDIO_INPUT_SAMPLE_RATE / AUDIO_OUTPUT_SAMPLE_RATE
        .channel = 1,         // ← mono, как в I2S config
        .bits_per_sample = 16,
    };

    
    // Включаем каналы
    /*esp_err_t ret = i2s_channel_enable(i2s_tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG,"i2c channel enable failed (i2s_tx_handle)");
        return ESP_FAIL;
    }*/
    

    esp_err_t ret = esp_codec_dev_open(audio_dev_es8311.play_dev, &fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open codec device (audio_dev_es8311.play_dev)");
        return ESP_FAIL;
    }

    esp_codec_dev_set_out_mute(audio_dev_es8311.play_dev, false);
    // Сброс флага на случай, если вызов не из task
    if (bytes_played_ptr) *bytes_played_ptr = 0;

    audio_start_play_from_mem_sys_msg_t msg = { .buffer = buffer, .size = size };
    project_event_send(APP_EVENT_AUDIO_START_PLAY_FROM_MEM, &msg, sizeof(msg));

    uint8_t *work_buf = heap_caps_malloc(1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!work_buf) return ESP_ERR_NO_MEM;

    uint32_t pos = 0;
    while (pos < size) {
        if (s_play_stop_requested) {
            ESP_LOGW(TAG, "⚠️ Playback stopped internally (mem)");
            s_play_stop_requested = false;
            esp_codec_dev_set_out_mute(audio_dev_es8311.play_dev, true);
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        }

        size_t to_send = (size - pos > 1024) ? 1024 : size - pos;
        memcpy(work_buf, buffer + pos, to_send);
        esp_codec_dev_write(audio_dev_es8311.play_dev, work_buf, to_send);
        pos += to_send;
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    heap_caps_free(work_buf);
    
    // Всегда возвращаем пройденный путь, даже при останове
    if (bytes_played_ptr) *bytes_played_ptr = pos;
    esp_codec_dev_set_out_mute(audio_dev_es8311.play_dev, true);
    vTaskDelay(pdMS_TO_TICKS(100));
    if (s_play_stop_requested) {
        // Событие отправит task, здесь просто помечаем и выходим
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "✅ Buffer playback complete: %lu bytes", size);
    audio_done_play_from_mem_sys_msg_t msg2 = { .size = size };
    project_event_send(APP_EVENT_AUDIO_DONE_PLAY_FROM_MEM, &msg2, sizeof(msg2));
    return ESP_OK;
}

// === Синхронная функция воспроизведения из файла ===

static esp_err_t audio_play_wav_file(const char *filename, uint32_t *bytes_played_ptr)
{
    s_play_stop_requested = false;
    if (!audio_dev_es8311.play_dev) {
        ESP_LOGE(TAG, "❌ Play device not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = AUDIO_OUTPUT_SAMPLE_RATE, // ← из board_config.h AUDIO_INPUT_SAMPLE_RATE / AUDIO_OUTPUT_SAMPLE_RATE
        .channel = 2,         // ← mono, как в I2S config
        .bits_per_sample = 16,
    };

    /*esp_err_t ret = i2s_channel_enable(i2s_tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG,"i2c channel tx init error");
        return ESP_FAIL;
    }*/

    esp_err_t ret = esp_codec_dev_open(audio_dev_es8311.play_dev, &fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open codec device (audio_dev_es8311.play_dev)");
        return ESP_FAIL;
    }

    esp_codec_dev_set_out_mute(audio_dev_es8311.play_dev, false);

    if (bytes_played_ptr) *bytes_played_ptr = 0;

    audio_start_play_from_file_sys_msg_t msg = { .filename = filename };
    project_event_send(APP_EVENT_AUDIO_START_PLAY_FROM_FILE, &msg, sizeof(msg));

    char path[64];
    snprintf(path, sizeof(path), "/sdcard/%s", filename);

    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s", path);
        return ESP_FAIL;
    }

    uint8_t *buf = heap_caps_malloc(1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        fclose(f);
        return ESP_FAIL;
    }

    size_t total_bytes = 0;
    while (!feof(f)) {
        if (s_play_stop_requested) {
            ESP_LOGW(TAG, "⚠️ Playback stopped internally (file)");
            s_play_stop_requested = false;
            //esp_codec_dev_close(audio_dev_es8311.play_dev);
            esp_codec_dev_set_out_mute(audio_dev_es8311.play_dev, true);
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        }

        size_t bytes = fread(buf, 1, 1024, f);
        if (bytes > 0) {
            esp_codec_dev_write(audio_dev_es8311.play_dev, buf, bytes);
            total_bytes += bytes;
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    fclose(f);
    heap_caps_free(buf);

    if (bytes_played_ptr) *bytes_played_ptr = total_bytes;
    //esp_codec_dev_close(audio_dev_es8311.play_dev);
    esp_codec_dev_set_out_mute(audio_dev_es8311.play_dev, true);
    vTaskDelay(pdMS_TO_TICKS(100));

    if (s_play_stop_requested) {
        // Событие отправит task, здесь просто помечаем и выходим
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "✅ Playback finished: %s", filename);
    audio_done_play_from_file_sys_msg_t msg2 = { .filename = filename };
    project_event_send(APP_EVENT_AUDIO_DONE_PLAY_FROM_FILE, &msg2, sizeof(msg2));
    return ESP_OK;
}

// === Одноразовая задача воспроизведения из памяти ===

typedef struct {
    const uint8_t *buffer;
    uint32_t size;
} play_from_mem_args_t;

static void task_play_from_mem(void *arg)
{
    play_from_mem_args_t *args = (play_from_mem_args_t *)arg;
    const uint8_t *buffer = args->buffer;
    uint32_t size = args->size;

    esp_codec_dev_mutex_lock(s_mutex, portMAX_DELAY);

    // ✅ СБРОС флага в начале задачи
    s_play_stop_requested = false;

    uint32_t bytes_played = 0;
    esp_err_t ret = audio_play_from_mem(buffer, size, &bytes_played);

    esp_codec_dev_mutex_unlock(s_mutex);

    // ✅ Остановка по флагу — отправляем STOPPED
    if (s_play_stop_requested) {
        ESP_LOGW(TAG, "⚠️ Playback stopped internally (mem), size=%lu bytes", bytes_played);
        esp_codec_dev_set_out_mute(audio_dev_es8311.play_dev, true);
        audio_stopped_play_mem_user_sys_msg_t msg = {
            .size = bytes_played,
        };

        esp_err_t send_ret = project_event_send(APP_EVENT_AUDIO_STOPPED_PLAY_MEM_USER, &msg, sizeof(msg));
        if (send_ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to send stop event: %d", send_ret);
        }

        heap_caps_free(args);
        vTaskDelay(pdMS_TO_TICKS(100));
        vTaskDelete(NULL);
        return;
    }

    // ✅ Нормальное завершение — срабатывает только при ret == ESP_OK
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Playback failed: %d", ret);
    }

    heap_caps_free(args);
    vTaskDelete(NULL);
}

esp_err_t audio_play_mem_async(const uint8_t *buffer, uint32_t size)
{
    if (!s_mutex) return ESP_FAIL;

    play_from_mem_args_t *args = heap_caps_malloc(sizeof(*args), MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
    if (!args) {
        ESP_LOGE(TAG, "❌ Failed to allocate args");
        return ESP_ERR_NO_MEM;
    }

    args->buffer = buffer;
    args->size = size;

    if (xTaskCreate(task_play_from_mem, "audio_play_mem", 4096, args, 5, NULL) != pdTRUE) {
        ESP_LOGE(TAG, "❌ Failed to create task for playback");
        heap_caps_free(args);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "▶️ Start async playback: %lu bytes", size);
    return ESP_OK;
}

// === Одноразовая задача воспроизведения из файла ===

typedef struct {
    const char *filename;
} play_file_args_t;

static void task_play_wav_file(void *arg)
{
    play_file_args_t *args = (play_file_args_t *)arg;

    esp_codec_dev_mutex_lock(s_mutex, portMAX_DELAY);

    // ✅ СБРОС флага в начале задачи
    s_play_stop_requested = false;

    uint32_t bytes_played = 0;
    esp_err_t ret = audio_play_wav_file(args->filename, &bytes_played);

    esp_codec_dev_mutex_unlock(s_mutex);

    // ✅ Остановка по флагу — отправляем STOPPED
    if (s_play_stop_requested) {
        ESP_LOGW(TAG, "⚠️ Playback stopped internally (file)");
        esp_codec_dev_set_out_mute(audio_dev_es8311.play_dev, true);
        audio_stopped_play_file_user_sys_msg_t msg = {
            .filename = args->filename
        };

        esp_err_t send_ret = project_event_send(APP_EVENT_AUDIO_STOPPED_PLAY_FILE_USER, &msg, sizeof(msg));
        if (send_ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to send stop event: %d", send_ret);
        }

        heap_caps_free(args);
        vTaskDelay(pdMS_TO_TICKS(100));
        vTaskDelete(NULL);
        return;
    }

    // ✅ Нормальное завершение
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Playback failed: %d", ret);
    }

    heap_caps_free(args);
    vTaskDelete(NULL);
}

esp_err_t audio_play_wav_file_async(const char *filename)
{
    if (!s_mutex) return ESP_FAIL;

    play_file_args_t *args = heap_caps_malloc(sizeof(*args), MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
    if (!args) {
        ESP_LOGE(TAG, "❌ Failed to allocate args");
        return ESP_ERR_NO_MEM;
    }

    args->filename = filename;

    if (xTaskCreate(task_play_wav_file, "audio_play_file", 4096, args, 5, NULL) != pdTRUE) {
        ESP_LOGE(TAG, "❌ Failed to create task for playback");
        heap_caps_free(args);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "▶️ Start async playback from file: %s", filename);
    return ESP_OK;
}