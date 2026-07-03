#ifndef LCD_ST7701_H
#define LCD_ST7701_H

#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_mipi_dsi.h"
//#include "esp_lvgl_port.h"

#define LCD_480_800_PANEL_60HZ_DPI_CONFIG(px_format)  \
    {                                                    \
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,     \
        .dpi_clock_freq_mhz = 28,                        \
        .virtual_channel = 0,                            \
        .in_color_format = px_format,                   \
        .num_fbs = 2,                                    \
        .video_timing = {                                \
            .h_size = 480,                               \
            .v_size = 800,                               \
            .hsync_back_porch = 42,                      \
            .hsync_pulse_width = 12,                     \
            .hsync_front_porch = 42,                     \
            .vsync_back_porch = 8,                      \
            .vsync_pulse_width = 2,                     \
            .vsync_front_porch = 166,                     \
        },                                               \
    }

esp_err_t init_lcd_mipi_dsi_st7701(void);
esp_err_t bsp_brightness_set(int brightness_percent);
void deinit_lcd_mipi_dsi_st7701(void);

#endif // INIT_LCD_MIPI_DSI_ST7701_H