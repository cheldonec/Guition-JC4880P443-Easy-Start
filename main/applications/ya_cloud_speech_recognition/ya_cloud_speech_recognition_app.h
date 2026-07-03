#ifndef YA_CLOUD_SPEECH_RECOGNITION_APP_H

#define YA_CLOUD_SPEECH_RECOGNITION_APP_H

#include "esp_err.h"
esp_err_t app_stt_demo_init(void);
void app_stt_demo_start(void);
void app_stt_demo_deinit(void);

#endif