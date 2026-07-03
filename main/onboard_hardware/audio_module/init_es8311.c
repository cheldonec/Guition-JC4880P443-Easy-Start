// main/onboard_hardware/audio_module/init_es8311.c
#include "init_es8311.h"
#include "esp_codec_dev_defaults.h"
#include "board_config.h"
#include "esp_log.h"

static const char *TAG = "AUDIO_CODEC_ES8311";

onboard_audio_device_handle_t audio_dev_es8311 = {0};

static const audio_codec_ctrl_if_t *s_ctrl_if = NULL;
static const audio_codec_gpio_if_t *s_gpio_if = NULL;

esp_err_t bsp_audio_codec_es8311_init(i2c_master_bus_handle_t i2c_bus_handle, i2s_chan_handle_t i2s_rx_handle, i2s_chan_handle_t i2s_tx_handle)
{
    ESP_LOGI(TAG, "Initializing ES8311 codec (shared ctrl_if)...");

    audio_dev_es8311.play_dev = NULL;
    audio_dev_es8311.rec_dev = NULL;

    const audio_codec_data_if_t *data_if = NULL;
    const audio_codec_if_t *codec_if = NULL;

    // I²C Control IF
    audio_codec_i2c_cfg_t i2c_cfg = {
        .bus_handle = i2c_bus_handle,
        .addr = AUDIO_CODEC_ES8311_ADDR,
    };
    s_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    if (!s_ctrl_if) {
        ESP_LOGE(TAG, "Failed to create I2C control interface");
        return ESP_FAIL;
    }

    // GPIO IF
    s_gpio_if = audio_codec_new_gpio();
    if (!s_gpio_if) {
        ESP_LOGE(TAG, "Failed to create GPIO interface");
        audio_codec_delete_ctrl_if(s_ctrl_if);
        s_ctrl_if = NULL;
        return ESP_FAIL;
    }

    // === DAC (PLAY) ===
    {
        audio_codec_i2s_cfg_t i2s_cfg = { .tx_handle = i2s_tx_handle };
        data_if = audio_codec_new_i2s_data(&i2s_cfg);
        if (!data_if) {
            ESP_LOGE(TAG, "Failed to create I2S TX data interface (PLAY)");
            goto cleanup;
        }

        es8311_codec_cfg_t es8311_dac_cfg = {
            .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
            .ctrl_if = s_ctrl_if,
            .gpio_if = s_gpio_if,
            .pa_pin = AUDIO_CODEC_PA_PIN,
            .use_mclk = false,
        };
        codec_if = es8311_codec_new(&es8311_dac_cfg);
        if (!codec_if) {
            ESP_LOGE(TAG, "Failed to create ES8311 codec (PLAY)");
            audio_codec_delete_data_if(data_if);
            goto cleanup;
        }

        esp_codec_dev_cfg_t dev_cfg = {
            .codec_if = codec_if,
            .data_if = data_if,
            .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        };

        audio_dev_es8311.play_dev = esp_codec_dev_new(&dev_cfg);
        if (!audio_dev_es8311.play_dev) {
            ESP_LOGE(TAG, "Failed to create PLAY device");
            audio_codec_delete_codec_if(codec_if);
            goto cleanup;
        }
        esp_codec_dev_set_out_vol(audio_dev_es8311.play_dev, 60.0);
    }

    // === ADC (REC) ===
    {
        audio_codec_i2s_cfg_t i2s_cfg = { .rx_handle = i2s_rx_handle };
        data_if = audio_codec_new_i2s_data(&i2s_cfg);
        if (!data_if) {
            ESP_LOGE(TAG, "Failed to create I2S RX data interface (REC)");
            goto cleanup;
        }

        es8311_codec_cfg_t es8311_adc_cfg = {
            .codec_mode = ESP_CODEC_DEV_WORK_MODE_ADC,
            .ctrl_if = s_ctrl_if,
            .gpio_if = s_gpio_if,
            .use_mclk = false,
        };
        codec_if = es8311_codec_new(&es8311_adc_cfg);
        if (!codec_if) {
            ESP_LOGE(TAG, "Failed to create ES8311 codec (REC)");
            audio_codec_delete_data_if(data_if);
            goto cleanup;
        }

        esp_codec_dev_cfg_t dev_cfg = {
            .codec_if = codec_if,
            .data_if = data_if,
            .dev_type = ESP_CODEC_DEV_TYPE_IN,
        };

        audio_dev_es8311.rec_dev = esp_codec_dev_new(&dev_cfg);
        if (!audio_dev_es8311.rec_dev) {
            ESP_LOGE(TAG, "Failed to create REC device");
            audio_codec_delete_codec_if(codec_if);
            goto cleanup;
        }

        esp_codec_dev_set_in_gain(audio_dev_es8311.rec_dev, 40.0);
    }

    // ✅ ДОБАВЛЕНО: Открываем устройства один раз при инициализации
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 16000, // ← из board_config.h AUDIO_INPUT_SAMPLE_RATE / AUDIO_OUTPUT_SAMPLE_RATE
        .channel = 1,         // ← mono, как в I2S config
        .bits_per_sample = 16,
    };

    if (audio_dev_es8311.rec_dev) {
        esp_err_t ret = esp_codec_dev_open(audio_dev_es8311.rec_dev, &fs);
        if (ret != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "❌ Failed to open rec_dev permanently: %d", ret);
            goto cleanup;
        }
        ESP_LOGI(TAG, "✅ rec_dev opened: %d Hz, %d ch, %d bps", fs.sample_rate, fs.channel, fs.bits_per_sample);
    }

    if (audio_dev_es8311.play_dev) {
        esp_err_t ret = esp_codec_dev_open(audio_dev_es8311.play_dev, &fs);
        if (ret != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "❌ Failed to open play_dev permanently: %d", ret);
            if (audio_dev_es8311.rec_dev) {
                esp_codec_dev_close(audio_dev_es8311.rec_dev); // ← закрываем rec_dev, если play_dev не удалось открыть
            }
            goto cleanup;
        }
        ESP_LOGI(TAG, "✅ play_dev opened: %d Hz, %d ch, %d bps", fs.sample_rate, fs.channel, fs.bits_per_sample);
    }

    ESP_LOGI(TAG, "ES8311 initialized: play_dev=%p, rec_dev=%p", audio_dev_es8311.play_dev, audio_dev_es8311.rec_dev);
    return ESP_OK;

cleanup:
    if (codec_if) audio_codec_delete_codec_if(codec_if);
    if (data_if)  audio_codec_delete_data_if(data_if);
    if (audio_dev_es8311.play_dev) {
        esp_codec_dev_delete(audio_dev_es8311.play_dev);
        audio_dev_es8311.play_dev = NULL;
    }
    if (audio_dev_es8311.rec_dev) {
        esp_codec_dev_delete(audio_dev_es8311.rec_dev);
        audio_dev_es8311.rec_dev = NULL;
    }
    if (s_ctrl_if) {
        audio_codec_delete_ctrl_if(s_ctrl_if);
        s_ctrl_if = NULL;
    }
    if (s_gpio_if) {
        audio_codec_delete_gpio_if(s_gpio_if);
        s_gpio_if = NULL;
    }
    return ESP_FAIL;
}

void bsp_audio_codec_es8311_deinit(void)
{
    // ✅ ДОБАВЛЕНО: Закрываем перед удалением
    if (audio_dev_es8311.rec_dev) {
        esp_codec_dev_close(audio_dev_es8311.rec_dev);
    }
    if (audio_dev_es8311.play_dev) {
        esp_codec_dev_close(audio_dev_es8311.play_dev);
    }

    if (audio_dev_es8311.play_dev) {
        esp_codec_dev_delete(audio_dev_es8311.play_dev);
        audio_dev_es8311.play_dev = NULL;
    }
    if (audio_dev_es8311.rec_dev) {
        esp_codec_dev_delete(audio_dev_es8311.rec_dev);
        audio_dev_es8311.rec_dev = NULL;
    }

    if (s_ctrl_if) {
        audio_codec_delete_ctrl_if(s_ctrl_if);
        s_ctrl_if = NULL;
    }
    if (s_gpio_if) {
        audio_codec_delete_gpio_if(s_gpio_if);
        s_gpio_if = NULL;
    }
    ESP_LOGI(TAG, "ES8311 deinitialized");
}