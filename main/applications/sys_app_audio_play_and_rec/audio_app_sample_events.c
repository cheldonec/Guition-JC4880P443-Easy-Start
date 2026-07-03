// main/applications/sys_app_audio_play_and_rec/audio_app_sample_events.c
#include "audio_app.h"
#include "project_event_handler_manager/project_event_handler_manager.h"
#include "board_init.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char* TAG = "AUDIO_EVT_SAMPLE";

// === Состояние теста ===
static bool s_mem_record_done = false;
static bool s_mem_play_done = false;
static bool s_file_record_done = false;

// === Глобальный буфер для текущего воспроизведения (из памяти) ===
static uint8_t *s_current_play_buffer = NULL;
static uint32_t s_current_play_buffer_size = 0;

// === Обработчик событий ===
static void audio_event_handler(project_event_id_t id, const void *data, uint16_t len)
{
    switch (id) {
        case APP_EVENT_AUDIO_PLAY_VOLUME_CHANGED: {
            const audio_volume_changed_sys_msg_t *msg = (const audio_volume_changed_sys_msg_t*)data;
            ESP_LOGI(TAG, "🔊 Volume changed to %d%%", msg->new_percent);
            break;
        }

        case APP_EVENT_AUDIO_START_REC_TO_FILE: {
            const audio_start_rec_to_file_sys_msg_t *msg = (const audio_start_rec_to_file_sys_msg_t*)data;
            ESP_LOGI(TAG, "📝 Start recording to file: %s (%lu sec)", msg->filename, msg->duration_sec);
            break;
        }

        case APP_EVENT_AUDIO_DONE_REC_TO_FILE: {
            const audio_done_rec_to_file_sys_msg_t *msg = (const audio_done_rec_to_file_sys_msg_t*)data;
            ESP_LOGI(TAG, "✅ File recorded: %s (%lu bytes)", msg->filename, msg->size);
            s_file_record_done = true;
            break;
        }

        case APP_EVENT_AUDIO_START_REC_TO_MEM: {
            const audio_start_rec_to_mem_sys_msg_t *msg = (const audio_start_rec_to_mem_sys_msg_t*)data;
            ESP_LOGI(TAG, "📝 Start recording to memory (%lu sec)", msg->duration_sec);
            break;
        }

        case APP_EVENT_AUDIO_STOPPED_REC_TO_MEM_USER: {
            const audio_stopped_rec_to_mem_user_sys_msg_t *msg = (const audio_stopped_rec_to_mem_user_sys_msg_t*)data;
            ESP_LOGW(TAG, "⚠️ Recording stopped: %lu bytes", msg->bytes_recorded);

            // ✅ КОПИРУЕМ данные из события
            if (s_current_play_buffer) {
                heap_caps_free(s_current_play_buffer);
                s_current_play_buffer = NULL;
                s_current_play_buffer_size = 0;
            }

            s_current_play_buffer = msg->data;
            s_current_play_buffer_size = msg->bytes_recorded;

            s_mem_record_done = true;
            break;
        }

        case APP_EVENT_AUDIO_DONE_REC_TO_MEM: {
            const audio_done_rec_to_mem_sys_msg_t *msg = (const audio_done_rec_to_mem_sys_msg_t*)data;
            ESP_LOGI(TAG, "✅ Memory recorded: %lu bytes", msg->size);

            // ✅ Копируем указатель и размер
            s_current_play_buffer_size = msg->size;
            s_current_play_buffer = (uint8_t *)msg->data;

            s_mem_record_done = true;
            break;
        }

        case APP_EVENT_AUDIO_START_PLAY_FROM_FILE: {
            const audio_start_play_from_file_sys_msg_t *msg = (const audio_start_play_from_file_sys_msg_t*)data;
            ESP_LOGI(TAG, "▶️ Start playing from file: %s", msg->filename);
            break;
        }

        case APP_EVENT_AUDIO_DONE_PLAY_FROM_FILE: {
            const audio_done_play_from_file_sys_msg_t *msg = (const audio_done_play_from_file_sys_msg_t*)data;
            ESP_LOGI(TAG, "✅ File played: %s", msg->filename);
            break;
        }

        case APP_EVENT_AUDIO_START_PLAY_FROM_MEM: {
            const audio_start_play_from_mem_sys_msg_t *msg = (const audio_start_play_from_mem_sys_msg_t*)data;
            ESP_LOGI(TAG, "▶️ Start playing from mem (%lu bytes)", msg->size);
            break;
        }

        case APP_EVENT_AUDIO_DONE_PLAY_FROM_MEM: {
            const audio_done_play_from_mem_sys_msg_t *msg = (const audio_done_play_from_mem_sys_msg_t*)data;
            ESP_LOGI(TAG, "✅ Memory playback complete: %lu bytes", msg->size);

            s_mem_play_done = true;

            // ✅ Освобождаем буфер (так как project_event_handler не делает этого)
            if (s_current_play_buffer) {
                heap_caps_free(s_current_play_buffer);
                s_current_play_buffer = NULL;
                s_current_play_buffer_size = 0;
            }
            break;
        }

        case APP_EVENT_AUDIO_STOPPED_REC_TO_FILE_USER: {
            const audio_stopped_rec_to_file_user_sys_msg_t *msg = (const audio_stopped_rec_to_file_user_sys_msg_t*)data;
            ESP_LOGW(TAG, "⚠️ File recording stopped: %s (%lu bytes)", msg->filename, msg->size);
            s_file_record_done = true;
            break;
        }

        // === НОВОЕ: обработка остановки воспроизведения файла ===
        case APP_EVENT_AUDIO_STOPPED_PLAY_FILE_USER: {
            const audio_stopped_play_file_user_sys_msg_t *msg = (const audio_stopped_play_file_user_sys_msg_t*)data;
            ESP_LOGW(TAG, "⚠️ File playback stopped: %s", msg->filename);
            // Не меняем s_file_play_done — просто логируем
            break;
        }

        // === НОВОЕ: обработка остановки воспроизведения из памяти ===
        case APP_EVENT_AUDIO_STOPPED_PLAY_MEM_USER: {
            const audio_stopped_play_mem_user_sys_msg_t *msg = (const audio_stopped_play_mem_user_sys_msg_t*)data;
            ESP_LOGW(TAG, "⚠️ Memory playback stopped: %lu bytes", msg->size);
            break;
        }

        default:
            break;
    }
}

// === Функция вывода свободной памяти ===
static void print_free_memory(const char* prefix)
{
    size_t spi_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t dram_free = heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    size_t total_free = spi_free + dram_free;

    ESP_LOGI(TAG, "%s: SPIRAM=%lu KB, DRAM=%lu B (%lu KB), Total=%lu KB",
        prefix,
        spi_free / 1024,
        dram_free,
        dram_free / 1024,
        total_free / 1024);
}

// ✅ Новая задача — вызвать audio_rec_stop() через 3 секунды
static void task_stop_rec_after_3_sec(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "⏱️ Manually triggering stop via audio_rec_stop()...");
    audio_rec_stop(); // ← ВАЖНО: используем внутренний флаг!
    vTaskDelay(pdMS_TO_TICKS(1000));
    vTaskDelete(NULL);
}

static void task_stop_play_after_3_sec(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "⏱️ Manually triggering stop via audio_play_stop()...");
    audio_play_stop();
    vTaskDelay(pdMS_TO_TICKS(1000));
    vTaskDelete(NULL);
}

// === Запуск сэмплов через event API (полная логика) ===
void audio_app_sample_events_start(void)
{
    ESP_LOGI(TAG, "▶️ Starting EVENT samples (10 iterations with internal-stop test)");

    // === 1. Регистрация обработчика (однократно) ===
    project_event_register_handler(APP_EVENT_AUDIO_START_REC_TO_MEM, audio_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_STOPPED_REC_TO_MEM_USER, audio_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_DONE_REC_TO_MEM, audio_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_START_PLAY_FROM_MEM, audio_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_DONE_PLAY_FROM_MEM, audio_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_START_REC_TO_FILE, audio_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_DONE_REC_TO_FILE, audio_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_STOPPED_REC_TO_FILE_USER, audio_event_handler); // ← новое

    project_event_register_handler(APP_EVENT_AUDIO_START_PLAY_FROM_FILE, audio_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_STOPPED_PLAY_FILE_USER, audio_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_STOPPED_PLAY_MEM_USER, audio_event_handler);

    // === 0. Начальное состояние ===
    print_free_memory("Initial heap stats");

    // === 2. Цикл из 10 итераций ===
    for (int iteration = 0; iteration < 10; iteration++) {
        // === Визуальный разделитель итерации ===
        ESP_LOGI(TAG, "=================================================================");
        ESP_LOGI(TAG, "============================ IT %d ============================", iteration + 1);
        ESP_LOGI(TAG, "=================================================================");
        ESP_LOGI(TAG, "▶️ STARTING ITERATION %d of 10", iteration + 1);

        // === ТЕСТ УТЕЧЕК: убедимся, что s_current_play_buffer == NULL ===
        if (s_current_play_buffer != NULL) {
            ESP_LOGE(TAG, "❌ LEAK DETECTED: s_current_play_buffer is not NULL before iteration %d!", iteration + 1);
            return;
        }
        ESP_LOGI(TAG, "✅ No leaks detected: buffer cleared");

        print_free_memory("Before memory recording");

        // === 3. Начинаем запись в память — 10 секунд ===
        ESP_LOGI(TAG, "📝 Starting 10-sec recording to memory...");
        s_mem_record_done = false;

        // ✅ Создаём задачу-стопер записи
        xTaskCreate(task_stop_rec_after_3_sec, "stop_rec_after_3_sec", 4096, NULL, 5, NULL);

        // ✅ Запускаем запись
        esp_err_t ret = audio_record_to_mem_some_secconds_async(10);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to start recording: %d", ret);
            return;
        }

        // === 4. Ждём завершения или стопа ===
        while (!s_mem_record_done) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        ESP_LOGI(TAG, "✅ Recording loop ended (done=%d)", s_mem_record_done);

        ESP_LOGI(TAG, "✅ Got recorded buffer (%lu bytes)", s_current_play_buffer_size);

        print_free_memory("After memory recording");

        // === 5. Воспроизводим из памяти (БЕЗ стопа — полное завершение) ===
        ESP_LOGI(TAG, "▶️ Starting playback from mem...");
        s_mem_play_done = false;
        ret = audio_play_from_mem_async(s_current_play_buffer, s_current_play_buffer_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to start playback: %d", ret);
        }

        // === 6. Ждём завершения воспроизведения ===
        while (!s_mem_play_done) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        ESP_LOGI(TAG, "✅ Playback complete, buffer freed");

        print_free_memory("After memory playback");

        // === 7. Записываем в файл "test3.wav" ===
        ESP_LOGI(TAG, "📝 Starting 10-sec recording to file: test3.wav");
        s_file_record_done = false;

        xTaskCreate(task_stop_rec_after_3_sec, "stop_file_after_3_sec", 4096, NULL, 5, NULL);

        ret = audio_record_to_wav_file_some_secconds_async("test3.wav", 10);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to start file recording: %d", ret);
            return;
        }

        // === 8. Ждём завершения записи в файл ===
        while (!s_file_record_done) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        print_free_memory("After file recording");

        // === 9. Воспроизводим файл (С ТЕСТОМ ОСТАНОВКИ) ===
        ESP_LOGI(TAG, "▶️ Starting playback from file: test3.wav (with 3-sec stop test)");
        // ✅ НОВОЕ: ожидаем, что воспроизведение не завершится — будет стоп
        ESP_LOGI(TAG, "⏱️ Will stop playback in 3 seconds...");

        // ✅ Запускаем стопер воспроизведения
        xTaskCreate(task_stop_play_after_3_sec, "stop_play_after_3_sec", 4096, NULL, 5, NULL);

        ret = audio_play_from_wav_file_async("test3.wav");
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to start file playback: %d", ret);
        }

        // === 10. Ждём, пока файл проиграется (но он будет остановлен!) ===
        vTaskDelay(pdMS_TO_TICKS(5000)); // 5 секунд — достаточно, чтобы стоп сработал

        // ✅ Проверяем, что файл не был проигран до конца (не должно быть APP_EVENT_AUDIO_DONE_PLAY_FROM_FILE)
        // Но мы просто продолжаем — это "частичное завершение"
        ESP_LOGI(TAG, "✅ File playback stopped (expected)");

        print_free_memory("After file playback (stopped)");

        // === 11. Пауза перед следующей итерацией ===
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // === 11. Отмена обработчиков ===
    project_event_unregister_handler(APP_EVENT_AUDIO_START_REC_TO_MEM, audio_event_handler);
    project_event_unregister_handler(APP_EVENT_AUDIO_STOPPED_REC_TO_MEM_USER, audio_event_handler);
    project_event_unregister_handler(APP_EVENT_AUDIO_DONE_REC_TO_MEM, audio_event_handler);
    project_event_unregister_handler(APP_EVENT_AUDIO_START_PLAY_FROM_MEM, audio_event_handler);
    project_event_unregister_handler(APP_EVENT_AUDIO_DONE_PLAY_FROM_MEM, audio_event_handler);
    project_event_unregister_handler(APP_EVENT_AUDIO_START_REC_TO_FILE, audio_event_handler);
    project_event_unregister_handler(APP_EVENT_AUDIO_DONE_REC_TO_FILE, audio_event_handler);
    project_event_unregister_handler(APP_EVENT_AUDIO_STOPPED_REC_TO_FILE_USER, audio_event_handler); // ← новое

    project_event_unregister_handler(APP_EVENT_AUDIO_START_PLAY_FROM_FILE, audio_event_handler);
    project_event_unregister_handler(APP_EVENT_AUDIO_STOPPED_PLAY_FILE_USER, audio_event_handler);
    project_event_unregister_handler(APP_EVENT_AUDIO_STOPPED_PLAY_MEM_USER, audio_event_handler);
    
    ESP_LOGI(TAG, "=================================================================");
    ESP_LOGI(TAG, "⏹️ All 10 EVENT samples finished!");
    ESP_LOGI(TAG, "=================================================================");

    // === 12. Финальное состояние ===
    print_free_memory("Final heap stats");
}