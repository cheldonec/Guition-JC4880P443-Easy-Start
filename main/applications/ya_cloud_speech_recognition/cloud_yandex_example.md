/**
 * ============================================================================
 * Яндекс SpeechKit — Полный пример интеграции с audio_engine
 * ============================================================================
 *
 * Описание: Как подключить Яндекс STT + TTS к ESP32:
 *   - Шаг 1: Запись аудио → буфер (без заголовка WAV)
 *   - Шаг 2: Отправка в STT API → получение текста
 *   - Шаг 3: Отправка текста в TTS API → получение WAV-без заголовка
 *   - Шаг 4: Воспроизведение через audio_engine
 *
 * Предполагается:
 *   - Используется audio_engine (bsp_audio_engine_record_to_mem, dispatch)
 *   - Есть Internet-подключение
 *   - Есть API-ключ и Folder ID в [Yandex Cloud Console](https://console.cloud.yandex.ru/)
 *
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>

// В твоём проекте: #include "audio/audio_codec_engine.h"
// В твоём проекте: #include "cloud_speech_recognition/cloud_yandex.h"

/**
 * ============================================================================
 * ШАГ 2: Обработчик события после записи — отправка в облако
 * ============================================================================
 */

static char s_recognized_text[256] = {0};

static void on_record_complete_for_cloud(void *handler_args, esp_event_base_t base,
                                         int32_t id, void *event_data)
{
    if (id != AUDIO_ENGINE_EVENT_START_RECORD_FILE) return;

    ESP_LOGI("CLOUD", "☁️ Sending to Yandex Cloud...");

    // 1️⃣ Записать 3 секунды → WAV-буфер (с заголовком WAV)
    uint32_t wav_size = 0;
    uint8_t *wav_buffer = bsp_audio_engine_record_to_mem(3, &wav_size);
    if (!wav_buffer || wav_size <= 44) {
        ESP_LOGE("CLOUD", "Recording failed");
        return;
    }

    // 2️⃣ Отправить STT: данные без заголовка WAV (buf + 44, size - 44)
    if (cloud_stt_yandex(
            wav_buffer + 44,          // ← без заголовка
            wav_size - 44,
            s_recognized_text,
            sizeof(s_recognized_text),
            "YOUR_YANDEX_API_KEY",    // ← замени на свой API-ключ
            "YOUR_FOLDER_ID") == ESP_OK) // ← замени на свой Folder ID
    {
        ESP_LOGI("CLOUD", "✅ Recognized: %s", s_recognized_text);

        // 3️⃣ Отправить текст в TTS → получить WAV-без заголовка
        uint8_t *tts_buffer = NULL;
        uint32_t tts_size = 0;
        if (cloud_tts_yandex(
                s_recognized_text,
                &tts_buffer,
                &tts_size,
                "YOUR_YANDEX_API_KEY",
                "YOUR_FOLDER_ID",
                "alya", "good") == ESP_OK)
        {
            // 4️⃣ Воспроизвести через audio_engine
            audio_cmd_t cmd = {
                .mode = AUDIO_ENGINE_EVENT_START_PLAY_FILE,
                .buffer = tts_buffer,
                .buffer_size = tts_size,
                .sample_info = {
                    .sample_rate = 16000,
                    .channel = 1,
                    .bits_per_sample = 16,
                },
            };

            if (bsp_audio_engine_dispatch(&cmd) == ESP_OK) {
                ESP_LOGI("CLOUD", "🔊 Playing TTS response...");
            }

            // Освободить память (tts_buffer — без заголовка WAV)
            heap_caps_free(tts_buffer);
        }
    }

    // Освободить WAV-буфер
    heap_caps_free(wav_buffer);
}

/**
 * ============================================================================
 * ШАГ 3: Регистрация обработчика в app_main()
 * ============================================================================
 */

void cloud_init_example(void)
{
    // После bsp_audio_engine_start() и до main loop:
    esp_event_handler_register(
        AUDIO_ENGINE_EVENTS,
        AUDIO_ENGINE_EVENT_START_RECORD_FILE,
        on_record_complete_for_cloud,
        NULL
    );

    ESP_LOGI("CLOUD", "✅ Yandex Cloud handler registered");
}

/**
 * ============================================================================
 * ПРИМЕР ИСПОЛЬЗОВАНИЯ
 * ============================================================================
 */

// В main.c — в app_main() после инициализации audio_engine:
void app_main_cloud_integration_example(void)
{
    // ...
    bsp_audio_engine_start();
    cloud_init_example();  // ← вызов регистрации
    // ...
}

/**
 * ============================================================================
 * Дополнительно: Пример с кнопкой для старта
 * ============================================================================
 */

// Глобальная переменная
static bool s_cloud_ready = false;

// Обработчик кнопки BOOT → начать запись
static void on_boot_button_for_cloud(bool enabled)
{
    if (!enabled || !s_cloud_ready) return;

    ESP_LOGI("CLOUD", "BOOT pressed → start cloud recognition");

    // Отправить команду записи
    audio_cmd_t cmd = {
        .mode = AUDIO_ENGINE_EVENT_START_RECORD_FILE,
        .file_path = "temp.wav", // без意义 — для совместимости
        .duration_sec = 3,
    };
    bsp_audio_engine_dispatch(&cmd);
}

// В cloud_init_example() добавь:
// bsp_audio_engine_set_monitor_cb(on_boot_button_for_cloud);
// s_cloud_ready = true;  // после успешной проверки Wi-Fi

/**
 * ============================================================================
 * ВАЖНО: Подготовка API
 * ============================================================================
 *
 * 1️⃣ Создать облако:
 *    https://console.cloud.yandex.ru/
 *
 * 2️⃣ Создать API-ключ:
 *    IAM → Keys → Create → "Api-Key"
 *
 * 3️⃣ Получить Folder ID:
 *    Overview → Folder ID
 *
 * 4️⃣ Выставить переменные:
 *    - В `cloud_yandex.c` замени "YOUR_YANDEX_API_KEY" и "YOUR_FOLDER_ID"
 *    - ИЛИ используй `idf.py menuconfig` → `YANDEX_API_KEY`, `YANDEX_FOLDER_ID`
 *
 * 5️⃣ Убедись:
 *    - ESP32 подключён к Wi-Fi
 *    - Время на ESP32 правильное (NTP)
 *
 * ============================================================================
 */

/**
 * ============================================================================
 * ЧЕК-ЛИСТ ОШИБОК
 * ============================================================================
 *
 * ❌ "Failed to init HTTP client"
 *    → Нет интернета, проверь Wi-Fi
 *
 * ❌ "STT returned 401"
 *    → Неверный API-ключ (проверь IAM)
 *
 * ❌ "STT returned 400"
 *    → Неверный формат аудио (нужен ogg/opus, не raw PCM)
 *       → В `cloud_yandex.c` нужно реализовать конвертацию PCM → OGG
 *
 * ❌ "Empty STT response"
 *    → Язык не распознан, попробуй "ru-RU"
 *
 * ============================================================================
 */

/**
 * ============================================================================
 * УЛУЧШЕНИЕ: Конвертация PCM → OGG/OPUS (для продакшена)
 * ============================================================================
 *
 * 1. Включи библиотеку `esp_opus`:
 *    menuconfig → Component config → Audio → Support for Opus
 *
 * 2. Используй:
 *    #include "esp_opus_encoder.h"
 *    opus_encoder_create(...)
 *    opus_encode(...)
 *
 * 3. В `cloud_yandex.c` вместо `dummy_ogg`:
 *    uint8_t ogg_frame[128];
 *    int frame_size = opus_encode(encoder, pcm_data, 480, ogg_frame, sizeof(ogg_frame));
 *    esp_http_client_set_post_field(client, ogg_frame, frame_size);
 *
 * ============================================================================
 */