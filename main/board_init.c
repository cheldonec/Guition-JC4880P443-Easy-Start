// board_init.c — единая точка входа для инициализации
#include "board_init.h"

#include "nvs_flash.h"
#include "esp_event.h"

#include "esp_log.h"

static const char* TAG = "INIT_MODULES";

esp_err_t bsp_init(void)
{
    ESP_LOGI(TAG, "=== Starting full system initialization ===");

    // Системные компоненты — критичные
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_LOGI(TAG, "Event loop created");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }
    ESP_LOGI(TAG, "NVS initialized");

    ESP_LOGI(TAG, "Initializing event handler...");
    ESP_ERROR_CHECK(project_event_init());
    ESP_LOGI(TAG, "Event handler initialized");
    
    // 1. I²C
    ESP_LOGI(TAG, "Initializing I2C...");
    ESP_ERROR_CHECK(bsp_init_i2c());
    ESP_LOGI(TAG, "I2C initialized");

    // 2. I²S
    ESP_LOGI(TAG, "Initializing I2S...");
    ESP_ERROR_CHECK(bsp_init_i2s());
    ESP_LOGI(TAG, "I2S initialized");

    // ✅ 3. Аудио: запись и воспроизведение
    ESP_LOGI(TAG, "Initializing audio codecs...");
    ESP_ERROR_CHECK(bsp_audio_codec_es8311_init(i2c_bus_handle, i2s_rx_handle, i2s_tx_handle));
    ESP_LOGI(TAG, "Audio codecs initialized");

    // 5. SD-карта
    ESP_LOGI(TAG, "Initializing SD card...");
    ESP_ERROR_CHECK(bsp_sd_card_io_init());
    ESP_LOGI(TAG, "SD card initialized");

    // 6. LCD тачскрин (GT911)
    ESP_LOGI(TAG, "Initializing LCD touch (GT911)...");
    ESP_ERROR_CHECK(init_lcd_touch_gt911(i2c_bus_handle));
    ESP_LOGI(TAG, "LCD touch initialized");

    // 7. LCD дисплей (ST7701)
    ESP_LOGI(TAG, "Initializing LCD (ST7701)...");
    ESP_ERROR_CHECK(init_lcd_mipi_dsi_st7701());
    ESP_LOGI(TAG, "LCD initialized");

    // 8. Wi-Fi (esp_netif + esp_wifi) — ЦЕНТРАЛИЗОВАННО
    ESP_LOGI(TAG, "Initializing Wi-Fi...");
    ESP_ERROR_CHECK(init_netif_STA());
    ESP_LOGI(TAG, "Wi-Fi initialized");


    ESP_LOGI(TAG, "=== All modules initialized successfully ===");
    return ESP_OK;
}

void deinit_all_modules(void)
{
    ESP_LOGI(TAG, "=== Starting deinitialization ===");

    // Деинициализация — в обратном порядке
    deinit_netif_STA();
    deinit_lcd_mipi_dsi_st7701();
    bsp_sd_card_io_deinit();
    bsp_audio_codec_es8311_deinit();
    bsp_deinit_i2s();
    bsp_deinit_i2c();

    ESP_LOGI(TAG, "=== All modules deinitialized ===");
}