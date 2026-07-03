#ifndef AUDIO_REC_H

#define AUDIO_REC_H

#include "esp_err.h"
#include "board_init.h"

void audio_rec_init(void);

esp_err_t audio_rec_record_to_wav_file_async(const char *filename, uint32_t duration_sec);

esp_err_t audio_rec_record_to_mem_async(uint32_t duration_sec);

void audio_rec_record_stop(void);



#endif