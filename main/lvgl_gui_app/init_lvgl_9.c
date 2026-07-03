#include "init_lvgl_9.h"
#include "esp_lvgl_port.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
//#include "../touch_screen/touch_gt911.h"
//#include "../touch_screen/lcd_st7701.h"
#include "lv_demos.h"

static const char* TAG = "INIT_LVGL_MODULE";

/* LVGL display and touch */
lv_display_t *lvgl_disp = NULL;
lv_indev_t *lvgl_touch_indev = NULL;

void init_lvgl_9(esp_lcd_panel_io_handle_t lcd_panel_io, esp_lcd_panel_handle_t disp_panel, esp_lcd_touch_handle_t touch_handle)
{
     ESP_LOGI(TAG, "Init LVGL");
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 6,         /* LVGL task priority */
        .task_stack = 65536,         /* LVGL task stack size */
        #if CONFIG_IDF_TARGET_ESP32P4
        .task_affinity = 1,        /* LVGL task pinned to core (-1 is no affinity) */
        .task_stack_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT | MALLOC_CAP_DMA,
        #else
        .task_affinity = -1,        /* LVGL task pinned to core (-1 is no affinity) */
        #endif
        .task_max_sleep_ms = 100,   /* Maximum sleep in LVGL task */
        .timer_period_ms = 5        /* LVGL timer tick period in ms */
    };
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    //init_lcd_touch_gt911();
    //init_lcd_mipi_dsi_st7701();
    ESP_LOGI(TAG, "Install MIPI DSI LCD TO LVGL");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lcd_panel_io,
        .panel_handle = disp_panel,
        .control_handle = NULL,
        .buffer_size = 480 * 800 * 2,
        .double_buffer = LCD_DRAW_BUFF_DOUBLE,
        //.double_buffer = 1,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .trans_size = 64,
        /* Rotation values must be same as used in esp_lcd for initial settings of the screen */
        .rotation = {
            .swap_xy = true,
            .mirror_x = false,
            .mirror_y = false,
        },
        #if LVGL_VERSION_MAJOR >= 9
        #if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
                .color_format = LV_COLOR_FORMAT_RGB888,
        #else
                .color_format = LV_COLOR_FORMAT_RGB565,
        #endif
        #endif
                .flags = {
                    .buff_dma =true,
                    .buff_spiram = true, // isused psram
                    //.buff_spiram = false, 
        #if LVGL_VERSION_MAJOR >= 9
                    .swap_bytes = (LCD_BIGENDIAN ? true : false),
        #endif
        #if CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR
                    .sw_rotate = false,                /* Avoid tearing is not supported for SW rotation */
        #else
                    .sw_rotate = true, /* Only SW rotation is supported for 90° and 270° */
        #endif
        //#if CONFIG_BSP_DISPLAY_LVGL_FULL_REFRESH
        //            .full_refresh = true,
        //#elif CONFIG_BSP_DISPLAY_LVGL_DIRECT_MODE
        //            .direct_mode = true,
        //#endif
        }
    };
    const lvgl_port_display_dsi_cfg_t dpi_cfg = {
        .flags = {
        #if CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR
            .avoid_tearing = true,
        #else
            .avoid_tearing = false,
        #endif
        }
    };
    lvgl_disp = lvgl_port_add_disp_dsi(&disp_cfg, &dpi_cfg);

    ESP_LOGI(TAG, "Install TOUCH TO LVGL");
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);

    
}