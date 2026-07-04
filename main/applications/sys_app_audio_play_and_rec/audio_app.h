#ifndef AUDIO_APP_H

#define AUDIO_APP_H

#include "esp_err.h"
#include "board_init.h"

// TEST and SAMPLES
// === Инициализация и запуск сэмплов ===

void audio_app_sample_events_start(void);

void audio_app_init(void);

esp_err_t audio_play_device_set_volume(int percent);

esp_err_t audio_rec_device_set_gane(float gane);

int audio_play_device_get_volume(void);

float audio_rec_device_get_gain(void);

// Запись в память (асинхронно)
esp_err_t audio_record_to_mem_some_secconds_async(uint32_t duration_sec);

// Запись в файл (асинхронно)
esp_err_t audio_record_to_wav_file_some_secconds_async(const char *filename, uint32_t duration_sec);

// Воспроизведение из файла (асинхронно)
esp_err_t audio_play_from_wav_file_async(const char *filename);

// Воспроизведение из памяти (асинхронно)
esp_err_t audio_play_from_mem_async(const uint8_t *buffer,uint32_t size);

void audio_rec_stop(void);

void audio_play_stop(void);

#endif