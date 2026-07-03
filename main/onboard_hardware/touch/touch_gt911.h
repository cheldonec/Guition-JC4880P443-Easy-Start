// init_touch_gt911.h
#ifndef TOUCH_GT911_H
#define TOUCH_GT911_H

#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

/**
 * @brief Глобальный хендл сенсорного контроллера GT911
 * @note Инициализируется через init_lcd_touch_gt911() и используется в LVGL
 */
//extern esp_lcd_touch_handle_t touch_handle;

/**
 * @brief Инициализация сенсорного экрана GT911 на существующей I²C-шине
 * @note Требует, чтобы i2c_bus_handle был уже инициализирован в init_i2c()
 * @return ESP_OK при успехе, иначе код ошибки
 */
esp_err_t init_lcd_touch_gt911(i2c_master_bus_handle_t i2c_bus_handle);

#endif // INIT_TOUCH_GT911_H