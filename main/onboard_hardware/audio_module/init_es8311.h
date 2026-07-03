// main/audio/init_es8311.h
#ifndef INIT_ES8311_H
#define INIT_ES8311_H

#include "esp_err.h"
#include "esp_codec_dev.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"

// === Структура для ES8311 ===
typedef struct {
    esp_codec_dev_handle_t play_dev; // DAC
    esp_codec_dev_handle_t rec_dev;  // ADC
} onboard_audio_device_handle_t;

// === Прототипы ===
esp_err_t bsp_audio_codec_es8311_init(i2c_master_bus_handle_t i2c_bus_handle, i2s_chan_handle_t i2s_rx_handle,i2s_chan_handle_t i2s_tx_handle);

void bsp_audio_codec_es8311_deinit(void);


#endif // INIT_ES8311_H