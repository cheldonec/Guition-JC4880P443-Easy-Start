#ifndef INIT_LVGL_9_H
#define INIT_LVGL_9_H
#include "esp_lvgl_port.h"

extern lv_display_t                 *lvgl_disp;
extern lv_indev_t                   *lvgl_touch_indev;
void init_lvgl_9(esp_lcd_panel_io_handle_t lcd_panel_io, esp_lcd_panel_handle_t disp_panel, esp_lcd_touch_handle_t touch_handle);

#endif