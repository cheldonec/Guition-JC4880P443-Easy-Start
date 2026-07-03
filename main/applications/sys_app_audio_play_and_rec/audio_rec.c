#include "audio_rec.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "project_event_handler_manager/project_event_handler_manager.h"
#include "esp_log.h"
#include "esp_codec_dev_os.h" 

static const char *TAG = "AUDIO_REC_MODULE";

static esp_codec_dev_mutex_handle_t s_mutex = NULL;

// === Глобальный статический буфер для записи (доступен и для таска, и для синхронных функций) ===
static uint8_t *s_rec_buffer = NULL;
static uint32_t s_rec_buffer_size = 0;

// === Внутренний флаг остановки записи ===
static bool s_rec_stop_requested = false; // по умолчанию false

void audio_rec_init(void)
{
    if (!s_mutex) {
        s_mutex = esp_codec_dev_mutex_create();
        if (s_mutex) ESP_LOGI(TAG, "✅ Mutex created");
        else ESP_LOGE(TAG, "❌ Failed to create mutex");
    }
}

void audio_rec_record_stop(void)
{
    s_rec_stop_requested = true;
    ESP_LOGI(TAG, "🛑 Запрошена остановка записи");
}

static esp_err_t audio_record_to_mem_some_secconds(uint32_t duration_sec, uint32_t *out_size)
{
    if (!audio_dev_es8311.rec_dev) {
        ESP_LOGE(TAG, "❌ Record device not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!duration_sec) {
        ESP_LOGE(TAG, "❌ Duration must be > 0");
        return ESP_ERR_INVALID_ARG;
    }

    // ✅ Отправляем событие "начало записи в память"
    audio_start_rec_to_mem_sys_msg_t start_msg = { .duration_sec = duration_sec };
    project_event_send(APP_EVENT_AUDIO_START_REC_TO_MEM, &start_msg, sizeof(start_msg));

    uint32_t estimated_size = 16000 * 2 * duration_sec + 512;

    // === Выделяем буфер, если не хватает или не выделен ===
    if (!s_rec_buffer || s_rec_buffer_size < estimated_size) {
        if (s_rec_buffer) {
            heap_caps_free(s_rec_buffer);
            s_rec_buffer = NULL;
        }
        s_rec_buffer = heap_caps_malloc(estimated_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_rec_buffer) {
            ESP_LOGE(TAG, "❌ Failed to allocate %lu bytes", estimated_size);
            return ESP_ERR_NO_MEM;
        }
        s_rec_buffer_size = estimated_size;
    }

    ESP_LOGI(TAG, "▶️ Recording %lu sec to local buffer (max %lu bytes)...", duration_sec, estimated_size);

    uint32_t total_size = 0;
    uint32_t max_samples = 16000 * duration_sec;
    uint8_t temp_buf[512];

    while (total_size / 2 < max_samples) {
        // ✅ Только внутренний флаг
        if (s_rec_stop_requested) {
            ESP_LOGW(TAG, "⚠️ Recording stopped internally");
            break;
        }

        int read_ret = esp_codec_dev_read(audio_dev_es8311.rec_dev, temp_buf, sizeof(temp_buf));
        if (read_ret == ESP_CODEC_DEV_OK) {
            if (total_size + sizeof(temp_buf) <= estimated_size) {
                memcpy(s_rec_buffer + total_size, temp_buf, sizeof(temp_buf));
                total_size += sizeof(temp_buf);
            } else {
                ESP_LOGW(TAG, "⚠️ Buffer full, truncating");
                break;
            }
        } else {
            ESP_LOGW(TAG, "⚠️ Read error: %d", read_ret);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    // Оптимизация размера (необязательно)
    if (total_size < estimated_size) {
        uint8_t *shrunk = heap_caps_realloc(s_rec_buffer, total_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (shrunk) {
            s_rec_buffer = shrunk;
            s_rec_buffer_size = total_size;
        }
    }

    *out_size = total_size;
    ESP_LOGI(TAG, "✅ Recording complete: %lu bytes", total_size);
    return ESP_OK;
}

// === Одноразовая задача записи в память ===
typedef struct {
    uint32_t duration_sec;
} record_to_mem_args_t;

static void task_record_to_mem(void *arg)
{
    record_to_mem_args_t *args = (record_to_mem_args_t *)arg;
    uint32_t duration_sec = args->duration_sec;

    // ✅ СБРОС флага в начале задачи
    s_rec_stop_requested = false;

    esp_codec_dev_mutex_lock(s_mutex, portMAX_DELAY);

    uint32_t size = 0;
    esp_err_t ret = audio_record_to_mem_some_secconds(duration_sec, &size);

    esp_codec_dev_mutex_unlock(s_mutex);

    // ✅ Остановка по внутреннему флагу
    if (s_rec_stop_requested) {
        ESP_LOGW(TAG, "⚠️ Recording stopped internally, size=%lu bytes", size);

        // ✅ Делаем копию, даже если запись прервана
        uint8_t *data_copy = NULL;
        if (s_rec_buffer && size > 0) {
            data_copy = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (data_copy) {
                memcpy(data_copy, s_rec_buffer, size);
            } else {
                ESP_LOGE(TAG, "❌ Failed to allocate copy (%lu bytes)", size);
            }
        }

        // Очищаем s_rec_buffer
        if (s_rec_buffer) {
            heap_caps_free(s_rec_buffer);
            s_rec_buffer = NULL;
            s_rec_buffer_size = 0;
        }

        // ✅ Отправляем событие С КОПИЕЙ
        audio_stopped_rec_to_mem_user_sys_msg_t msg = {
            .bytes_recorded = size,
            .data = data_copy // ← ДОБАВЛЕНО!
        };

        esp_err_t send_ret = project_event_send(APP_EVENT_AUDIO_STOPPED_REC_TO_MEM_USER, &msg, sizeof(msg));
        if (send_ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to send stop event: %d", send_ret);
            if (data_copy) heap_caps_free(data_copy);
        }

        heap_caps_free(args);
        vTaskDelete(NULL);
        return;
    }

    // ✅ Нормальное завершение
    if (ret == ESP_OK && s_rec_buffer && size > 0) {
        uint8_t *data_copy = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!data_copy) {
            ESP_LOGE(TAG, "❌ Failed to allocate copy buffer (%lu bytes)", size);
            ret = ESP_ERR_NO_MEM;
        } else {
            memcpy(data_copy, s_rec_buffer, size);

            // ✅ Отправляем событие с копией
            audio_done_rec_to_mem_sys_msg_t msg = {
                .size = size,
                .data = data_copy
            };

            ret = project_event_send(APP_EVENT_AUDIO_DONE_REC_TO_MEM, &msg, sizeof(msg));
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "❌ Failed to send event: %d", ret);
                heap_caps_free(data_copy);
            }
        }
    } else {
        ESP_LOGE(TAG, "❌ Recording failed: %d", ret);
    }

    // ✅ Очищаем s_rec_buffer (в любом случае после работы)
    if (s_rec_buffer) {
        heap_caps_free(s_rec_buffer);
        s_rec_buffer = NULL;
        s_rec_buffer_size = 0;
    }

    heap_caps_free(args);
    vTaskDelete(NULL);
}

esp_err_t audio_rec_record_to_mem_async(uint32_t duration_sec)
{
    if (!s_mutex) return ESP_FAIL;

    record_to_mem_args_t *args = heap_caps_malloc(sizeof(*args), MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
    if (!args) {
        ESP_LOGE(TAG, "❌ Failed to allocate args");
        return ESP_ERR_NO_MEM;
    }

    args->duration_sec = duration_sec;

    if (xTaskCreate(task_record_to_mem, "audio_rec_mem", 4096, args, 5, NULL) != pdTRUE) {
        ESP_LOGE(TAG, "❌ Failed to create task for recording");
        heap_caps_free(args);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "▶️ Start async recording: %lu sec", duration_sec);
    return ESP_OK;
}

static esp_err_t audio_record_to_wav_file_some_seconds(const char *filename, uint32_t duration_sec, uint32_t *out_size)
{
    if (!audio_dev_es8311.rec_dev) {
        ESP_LOGE(TAG, "❌ Record device not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    char path[64];
    snprintf(path, sizeof(path), "/sdcard/%s", filename);

    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s", path);
        return ESP_FAIL;
    }

    // Записываем WAV-заголовок (44 байта)
    fwrite("RIFF", 1, 4, f);
    fwrite("\0\0\0\0", 1, 4, f); // size
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    fwrite("\x10\0\0\0", 1, 4, f);
    fwrite("\x01\0", 1, 2, f);  // PCM
    fwrite("\x01\0", 1, 2, f);  // 1 channel
    fwrite("\x40\x3d\0\0", 1, 4, f); // 16000 Hz
    fwrite("\x80\x7d\0\0", 1, 4, f); // 32000 byte/sec
    fwrite("\x02\0", 1, 2, f);  // 2 bytes/sample
    fwrite("\x10\0", 1, 2, f);  // 16 bits

    fwrite("data", 1, 4, f);
    fwrite("\0\0\0\0", 1, 4, f); // data size

    uint8_t *buf = heap_caps_malloc(1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        fclose(f);
        return ESP_FAIL;
    }

    size_t total_size = 0;
    uint32_t max_samples = 16000 * duration_sec;
    uint32_t samples = 0;

    while (samples < max_samples) {
        // ✅ Только внутренний флаг
        if (s_rec_stop_requested) {
            ESP_LOGW(TAG, "⚠️ File recording stopped internally");
            break;
        }

        int ret = esp_codec_dev_read(audio_dev_es8311.rec_dev, buf, 512);
        if (ret == ESP_CODEC_DEV_OK) {
            size_t bytes_written = fwrite(buf, 1, 512, f);
            if (bytes_written == 0) {
                ESP_LOGE(TAG, "fwrite failed, stopping recording");
                break;
            }
            total_size += bytes_written;
            samples += bytes_written / 2;
        } else {
            ESP_LOGE(TAG, "Read error: %d", ret);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    // Обновляем заголовок
    fseek(f, 40, SEEK_SET);
    fwrite(&total_size, 1, 4, f);
    fseek(f, 4, SEEK_SET);
    uint32_t riff_size = total_size + 36;
    fwrite(&riff_size, 1, 4, f);

    fclose(f);
    heap_caps_free(buf);

    *out_size = total_size;
    ESP_LOGI(TAG, "✅ Recorded %lu bytes to %s", total_size, filename);
    return ESP_OK;
}

// === Одноразовая задача записи в файл ===
typedef struct {
    const char *filename;
    uint32_t duration_sec;
} record_to_file_args_t;

static void task_record_to_file(void *arg)
{
    record_to_file_args_t *args = (record_to_file_args_t *)arg;

    // ✅ СБРОС флага в начале задачи
    s_rec_stop_requested = false;

    esp_codec_dev_mutex_lock(s_mutex, portMAX_DELAY);

    uint32_t size = 0;
    esp_err_t ret = audio_record_to_wav_file_some_seconds(args->filename, args->duration_sec, &size);

    esp_codec_dev_mutex_unlock(s_mutex);

    // ✅ Остановка по внутреннему флагу
    if (s_rec_stop_requested) {
        ESP_LOGW(TAG, "⚠️ File recording stopped internally, size=%lu", size);

        // ✅ Отправляем событие о стопе — без data (файл уже на SD)
        audio_stopped_rec_to_file_user_sys_msg_t msg = {
            .size = size,
            .filename = args->filename
        };
        esp_err_t send_ret = project_event_send(APP_EVENT_AUDIO_STOPPED_REC_TO_FILE_USER, &msg, sizeof(msg));
        if (send_ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to send stop event: %d", send_ret);
        }

        heap_caps_free(args);
        vTaskDelete(NULL);
        return;
    }

    if (ret == ESP_OK) {
        audio_done_rec_to_file_sys_msg_t msg = { .size = size, .filename = args->filename };
        project_event_send(APP_EVENT_AUDIO_DONE_REC_TO_FILE, &msg, sizeof(msg));
    } else {
        ESP_LOGE(TAG, "❌ File recording failed: %d", ret);
    }

    heap_caps_free(args);
    vTaskDelete(NULL);
}

// === Асинхронная функция записи в файл ===

esp_err_t audio_rec_record_to_wav_file_async(const char *filename, uint32_t duration_sec)
{
    if (!s_mutex) return ESP_FAIL;

    record_to_file_args_t *args = heap_caps_malloc(sizeof(*args), MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
    if (!args) {
        ESP_LOGE(TAG, "❌ Failed to allocate args");
        return ESP_ERR_NO_MEM;
    }

    args->filename = filename;
    args->duration_sec = duration_sec;

    if (xTaskCreate(task_record_to_file, "audio_rec_file", 4096, args, 5, NULL) != pdTRUE) {
        ESP_LOGE(TAG, "❌ Failed to create task for recording");
        heap_caps_free(args);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "▶️ Start async recording to file: %s (%lu sec)", filename, duration_sec);
    return ESP_OK;
}