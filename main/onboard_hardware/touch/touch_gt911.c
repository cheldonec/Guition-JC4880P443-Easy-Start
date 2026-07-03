// init_touch_gt911.c
#include "touch_gt911.h"
#include "esp_lcd_touch_gt911.h"
#include "board_config.h"

static const char* TAG = "INIT_TOUCH_GT911";

esp_lcd_touch_handle_t touch_handle = NULL;

// Локальный хендл для IO — не экспортируем (не нужен вне модуля)
static esp_lcd_panel_io_handle_t tp_io_handle = NULL;

esp_err_t init_lcd_touch_gt911(i2c_master_bus_handle_t i2c_bus_handle)
{
    ESP_LOGI(TAG, "Initializing GT911 touch controller...");

    // Проверка — I2C должна быть инициализирована
    if (!i2c_bus_handle) {
        ESP_LOGE(TAG, "I2C bus not initialized! Call init_i2c() first.");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t res = ESP_FAIL;

    // === 1. Создать I2C panel IO для GT911 ===
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_config.scl_speed_hz = I2C_CLK_SPEED_HZ;  // из config.h: 400 kHz

    res = esp_lcd_new_panel_io_i2c(i2c_bus_handle, &tp_io_config, &tp_io_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create GT911 I2C IO: %s", esp_err_to_name(res));
        goto cleanup;
    }
    ESP_LOGI(TAG, "GT911 I2C IO created");

    // === 2. Создать handle тачскрина ===
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = LCD_TOUCH_RST,
        .int_gpio_num = LCD_TOUCH_INT,
        .levels = {
            .reset = 0,         // Low-активный сброс
            .interrupt = 0,     // Low-активный прерывания
        },
        .flags = {
            .swap_xy = 0,       // не менять оси
            .mirror_x = 0,      // без зеркалирования
            .mirror_y = 0,
        },
    };

    res = esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &touch_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create GT911 touch: %s", esp_err_to_name(res));
        goto cleanup;
    }
    ESP_LOGI(TAG, "GT911 touch initialized (x=%d, y=%d)", LCD_H_RES, LCD_V_RES);

    return ESP_OK;

cleanup:
    // Если что-то пошло не так — освободить ресурсы
    if (tp_io_handle) {
        esp_lcd_panel_io_del(tp_io_handle);
        tp_io_handle = NULL;
    }
    touch_handle = NULL;
    return res;
}