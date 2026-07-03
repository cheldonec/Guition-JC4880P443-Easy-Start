#ifndef CLOUD_YANDEX_H
#define CLOUD_YANDEX_H

#include "esp_err.h"
#include <stdint.h>

esp_err_t cloud_test_http_get(char *result, size_t result_size);
esp_err_t cloud_test_http_post(char *result, size_t result_size);

/**
 * @brief Распознавание речи через Яндекс SpeechKit STT
 *
 * @param audio_data Аудиоданные (без WAV-заголовка)
 * @param audio_size Размер аудиоданных
 * @param[out] text buffer для текста (должен быть ≥ 256 байт)
 * @param text_size размер buffer
 * @param api_key Your Yandex API key
 * @param folder_id Your Yandex folder ID (для STT)
 * @return ESP_OK при успехе
 */
esp_err_t cloud_stt_yandex(
    const uint8_t *audio_data,
    uint32_t audio_size,
    char *text,
    size_t text_size,
    const char *api_key,
    const char *folder_id
);

/**
 * @brief Синтез речи через Яндекс SpeechKit TTS
 *
 * @param text Текст для синтеза
 * @param[out] wav_buffer_out Указатель на буфер с WAV-данными (нужно free через heap_caps_free)
 * @param[out] wav_size_out Размер буфера
 * @param api_key Your Yandex API key
 * @param folder_id Your Yandex folder ID (для TTS)
 * @param voice Имя голоса (например, "alya", "zahar", "omazh")
 * @param emotion Эмоция (например, "good", "evil", "neutral")
 * @return ESP_OK при успехе
 */
esp_err_t cloud_tts_yandex(
    const char *text,
    uint8_t **wav_buffer_out,
    uint32_t *wav_size_out,
    const char *api_key,
    const char *folder_id,
    const char *voice,
    const char *emotion
);

#endif // CLOUD_YANDEX_H