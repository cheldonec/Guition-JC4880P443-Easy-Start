// init_modules.h
#ifndef INIT_MODULES_H
#define INIT_MODULES_H

#include "project_event_handler_manager/project_event_handler_manager.h"
#include "onboard_hardware/i2c_if/init_i2c.h"
#include "onboard_hardware/i2s_if/init_i2s.h"
#include "onboard_hardware/audio_module/init_es8311.h"
#include "onboard_hardware/screen/lcd_st7701.h"
#include "onboard_hardware/touch/touch_gt911.h"
#include "onboard_hardware/storage/sd_card_io.h"
#include "onboard_hardware/net_if/init_netif.h"
#include "lvgl_gui_app/init_lvgl_9.h"
#include "esp_err.h"
#include "board_config.h"
//#include "driver/i2c_master.h"
//#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
//#include "driver/sd_host.h"



//#include "esp_lcd_mipi_dsi.h"
//#include "esp_lvgl_port.h"


// --- Глобальные экспортируемые handle'ы (все модули) ---
// I2C
extern i2c_master_bus_handle_t i2c_bus_handle;

// I2S
extern i2s_chan_handle_t i2s_tx_handle;
extern i2s_chan_handle_t i2s_rx_handle;

// Audio
//extern esp_codec_dev_handle_t record_dev;
//extern esp_codec_dev_handle_t play_dev;
extern onboard_audio_device_handle_t audio_dev_es8311;

// SD card
extern sdmmc_card_t *sd_card;

// LCD Touch (GT911)
extern esp_lcd_touch_handle_t touch_handle;


// LCD
extern esp_lcd_dsi_bus_handle_t     mipi_dsi_bus;
extern esp_lcd_panel_handle_t       disp_panel;
extern esp_lcd_panel_io_handle_t    lcd_panel_io;

/* LVGL display and touch */
extern lv_display_t                 *lvgl_disp;
extern lv_indev_t                   *lvgl_touch_indev;

/* WI-FI*/
extern esp_netif_t                  *sta_netif;
//extern esp_wifi_handle_t            wifi_handle;
extern bool s_wifi_connected;

/**
 * @brief Инициализация всех модулей: I2C, I2S, аудио, SD-карта, LCD-тач
 * @note Вызывает init_* по порядку. Если один модуль упал — возвращает ошибку.
 * @return ESP_OK при успехе, иначе код ошибки первого упавшего модуля
 */
esp_err_t bsp_init(void);

/**
 * @brief Деинициализация всех модулей в обратном порядке
 * @note Вызывает deinit_* модулей, где есть, иначе просто очищает handle'ы
 */
void deinit_all_modules(void);

#endif // INIT_MODULES_H