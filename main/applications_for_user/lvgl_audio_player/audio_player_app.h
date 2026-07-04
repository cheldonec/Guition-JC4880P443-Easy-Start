// main/applications/sys_app_audio_player/audio_player_app.h
#ifndef AUDIO_PLAYER_APP_H
#define AUDIO_PLAYER_APP_H

#include "esp_err.h"

esp_err_t audio_player_app_init(void);
void audio_player_app_start(void);
void audio_player_app_stop(void);

#endif