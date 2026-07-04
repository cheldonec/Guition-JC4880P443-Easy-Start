// main.c (обновлённая версия)
#include "esp_log.h"
#include "board_init.h"
#include "main_sys_event_handler.h"
#include "lvgl_gui_app/init_lvgl_9.h"
#include "lv_demos.h"

#include "esp_wifi.h"
#include "applications/sys_app_wifi_manager/wifi_manager_app.h"
//#include "applications/ya_cloud_speech_recognition/ya_cloud_speech_recognition_app.h"
#include "applications/sys_app_memory_monitor/memory_monitor_app.h"
#include "applications/sys_app_sd_card_info/sd_card_info_app.h"
#include "applications/sys_app_time_sync/time_sync_app.h"
#include "applications/sys_app_rcp_c6_update/rcp_c6_update_app.h"
#include "applications/sys_app_audio_play_and_rec/audio_app.h"
#include "project_event_handler_manager/project_event_handler_manager.h"
#include "applications_for_user/lvgl_audio_player/audio_player_app.h"
#include "cJSON.h"

static const char *TAG = "MAIN";


void app_main(void)
{
    esp_err_t err;
    // 🔧 1️⃣ Инициализация системы
    if (bsp_init() != ESP_OK) {
        ESP_LOGE(TAG, "❌ Full system init failed");
        return;
    }

    

    //🔧 2️⃣ Подписка на все системные сообщения ()
    main_sys_event_handler_register_all();
    
    

    // 🔧 3️⃣ LVGL init
    init_lvgl_9(lcd_panel_io, disp_panel, touch_handle);

    //lvgl_port_lock(0);
    //lv_demo_music();
    //lvgl_port_unlock();
    //ESP_LOGI(TAG, "🎨 LVGL demo shown");

    //SD_CARD
    app_sd_card_info_init();
    app_sd_card_info_start();

    // RCP C6 UPDATE
    app_rcp_c6_update_init();

    // INIT WIFI APP
    app_wifi_init();

    // SETUP WIFI CREDENTIALS 0 - last used network
    /*ESP_LOGI(TAG, "Saving AndroidAP network to bucket=0 SET FOR TEST");
    err = wifi_set_STA_credentials(0, "AndroidAP", "11111111");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to save AndroidAP: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "✅ AndroidAP saved to bucket=0");
    }*/

    ESP_LOGI(TAG, "Saving Keenetic-4201 network to bucket=1");
    err = wifi_set_STA_credentials(1, "Keenetic-4201", "5dJpcJdi");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to save Keenetic-4201: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "✅ Keenetic-4201 saved to bucket=1");
    }

    ESP_LOGI(TAG, "TRY TO START app_wifi_start_sta");
    esp_err_t start_sta_err = app_wifi_start_sta(sta_netif,"AndroidAP", "11111111");


    // ✅ Синхронизация времени
    ESP_ERROR_CHECK(app_time_sync_init(TZ_MSK)); // TZ_MSK - Moscow time zone
    app_time_sync_start();

    // ✅ Yandex STT
    // ESP_ERROR_CHECK(app_stt_demo_init());
    // app_stt_demo_start();

    // ✅ Мониторинг памяти
     ESP_ERROR_CHECK(app_memory_monitor_init());
     app_memory_monitor_start(5);

     //AAUDIO TEST
    //  AUDIO APP INIT
    audio_app_init();
    //vTaskDelay(pdMS_TO_TICKS(5000));
    // Event-примеры
    audio_play_device_set_volume(25);
    audio_rec_device_set_gane(30.0);
    //audio_app_sample_events_start();
    
    //audio_play_from_wav_file_async("sample_beat.wav");
    
    //USER APPLICATIONS
    // 🔧 Audio Player App
    audio_player_app_init();
    audio_player_app_start(); 

    ESP_LOGI(TAG, "✅ All applications started");

    

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(portMAX_DELAY));
    }
}