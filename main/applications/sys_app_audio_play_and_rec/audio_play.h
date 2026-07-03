// audio_play.h
#ifndef AUDIO_PLAY_H
#define AUDIO_PLAY_H

#include "esp_err.h"
#include "board_init.h"

void audio_play_init(void);

// Асинхронные функции (одноразовые таски)
esp_err_t audio_play_mem_async(const uint8_t *buffer, uint32_t size);
esp_err_t audio_play_wav_file_async(const char *filename);

void audio_play_player_stop(void); 

#endif