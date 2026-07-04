#include "init_i2s.h"
#include "board_config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_private/rtc_clk.h"
#include "driver/i2s_std.h"

static const char *TAG = "init_i2s_module";

// --- Инициализация I2S ---
i2s_chan_handle_t i2s_tx_handle = NULL;
i2s_chan_handle_t i2s_rx_handle = NULL;

esp_err_t bsp_init_i2s(void)
{
    
    ESP_LOGI(TAG, "Initializing I2S...");
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

    // Сначала создаём канал (можно дуплексный)
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &i2s_tx_handle, &i2s_rx_handle), TAG, "new i2s channel failed");

    // Слотовая конфигурация — для Philips/I2S
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_OUTPUT_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(16, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = AUDIO_I2S_GPIO_MCLK,
            .bclk = AUDIO_I2S_GPIO_BCLK,
            .ws   = AUDIO_I2S_GPIO_WS,
            .dout = AUDIO_I2S_GPIO_DOUT,
            .din  = AUDIO_I2S_GPIO_DIN,
        },
    };
    

    // Инициализируем оба направления (вход и выход)
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(i2s_tx_handle, &std_cfg), TAG, "tx init failed");
    ESP_LOGI(TAG, "tx init ok");
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(i2s_rx_handle, &std_cfg), TAG, "rx init failed");
    ESP_LOGI(TAG, "rx init ok");
    // Включаем каналы
    ESP_RETURN_ON_ERROR(i2s_channel_enable(i2s_tx_handle), TAG, "tx enable failed");
    ESP_LOGI(TAG, "tx enable ok");
    
    ESP_RETURN_ON_ERROR(i2s_channel_enable(i2s_rx_handle), TAG, "rx enable failed");
    ESP_LOGI(TAG, "rx enable ok");

    return ESP_OK;
}

void bsp_deinit_i2s(void)
{
    ESP_LOGI(TAG, "Deinitializing I2S...");
    if (i2s_tx_handle) {
        ESP_LOGI(TAG, "Deinitializing I2S TX...");
        i2s_channel_disable(i2s_tx_handle);
        i2s_del_channel(i2s_tx_handle);
    }
    if (i2s_rx_handle) {
        ESP_LOGI(TAG, "Deinitializing I2S RX...");
        i2s_channel_disable(i2s_rx_handle);
        i2s_del_channel(i2s_rx_handle);
    }
}