#include "audio_app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "audio_rec.h"
#include "audio_play.h"
#include "project_event_handler_manager/project_event_handler_manager.h"

static const char* TAG = "AUDIO_APP";

static int      s_volume = 10;
static float    s_gane = 10.0;


void audio_app_init(void)
{
    audio_rec_init();
    audio_play_init();
    ESP_LOGI(TAG, "✅ Audio app init (one-shot tasks + mutex)");
}

esp_err_t audio_rec_device_set_gane(float gane)
{
    
    int res = esp_codec_dev_set_in_gain(audio_dev_es8311.rec_dev, gane);
    if (res == ESP_CODEC_DEV_OK)
    {
        s_gane = gane;
        return ESP_OK;
    }
    return -ESP_ERR_INVALID_ARG;
}

esp_err_t audio_play_device_set_volume(int percent)
{
    esp_err_t ret = ESP_FAIL;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    int new_volume = percent;
    if (audio_dev_es8311.play_dev) {
        ret = esp_codec_dev_set_out_vol(audio_dev_es8311.play_dev, new_volume);
        if (ret == ESP_OK) {
            s_volume = new_volume;
            audio_volume_changed_sys_msg_t msg = { .new_percent = s_volume };
            project_event_send(APP_EVENT_AUDIO_PLAY_VOLUME_CHANGED, &msg, sizeof(msg));
        }
    }
    return ret;
}

int audio_play_device_get_volume(void)
{
    return s_volume;
}

float audio_rec_device_get_gain(void)
{
    return s_gane;
}


// === Асинхронные функции ===

esp_err_t audio_record_to_mem_some_secconds_async(uint32_t duration_sec)
{
    return audio_rec_record_to_mem_async(duration_sec);
}

esp_err_t audio_record_to_wav_file_some_secconds_async(const char *filename, uint32_t duration_sec)
{
    return audio_rec_record_to_wav_file_async(filename, duration_sec);
}

esp_err_t audio_play_from_wav_file_async(const char *filename)
{
    return audio_play_wav_file_async(filename);
}

esp_err_t audio_play_from_mem_async(const uint8_t *buffer,uint32_t size)
{
    return audio_play_mem_async(buffer, size);
}

void audio_rec_stop(void)
{
    return audio_rec_record_stop();
}

void audio_play_stop(void)
{
    return audio_play_player_stop();
}

