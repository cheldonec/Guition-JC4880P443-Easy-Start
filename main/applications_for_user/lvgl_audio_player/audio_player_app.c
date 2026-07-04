// main/applications_for_user/lvgl_audio_player/audio_player_app.c

#include "audio_player_app.h"
#include "lvgl_gui_app/init_lvgl_9.h"
#include "project_event_handler_manager/project_event_handler_manager.h"
#include "applications/sys_app_sd_card_info/sd_card_info_app.h"
#include "applications/sys_app_audio_play_and_rec/audio_app.h"
#include "esp_log.h"

static const char *TAG = "AUDIO_PLAYER_APP";

// Глобальные переменные
static lv_obj_t *list_obj = NULL;  // ← Теперь это просто lv_obj, а не lv_list
static lv_obj_t *info_label = NULL;
static char **file_names = NULL;
static uint16_t file_count = 0;

// Обработчик нажатия на кнопку (файл)
static void listbox_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        // Получаем id файла из user_data
        int id = (int)(intptr_t)lv_obj_get_user_data(btn);
        
        if (id >= 0 && id < file_count && file_names[id]) {
            ESP_LOGI(TAG, "▶️ Play file: %s", file_names[id]);
            audio_play_from_wav_file_async(file_names[id]);
            
            // Обновляем info_label
            lv_label_set_text_fmt(info_label, "▶️ Playing: %s", file_names[id]);
        }
    }
}

// Обработчик событий от SD-карты
static void handle_sd_file_list_event(const sd_file_list_lvgl_event_t *msg)
{
    if (file_names) {
        for (uint16_t i = 0; i < file_count; i++) {
            free(file_names[i]);
        }
        free(file_names);
    }

    file_count = msg->file_count;
    file_names = malloc(file_count * sizeof(char*));
    if (!file_names) return;

    lvgl_port_lock(0);

    // Очищаем список кнопок
    lv_obj_clean(list_obj);

    for (uint16_t i = 0; i < file_count; i++) {
        file_names[i] = strdup(&msg->filenames[i][0]);
        if (file_names[i]) {
            // Создаём кнопку и надпись
            lv_obj_t *btn = lv_btn_create(list_obj);
            lv_obj_set_size(btn, lv_pct(100), 40);
            lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, i * 45); // 45px spacing

            lv_obj_t *label = lv_label_create(btn);
            lv_label_set_text(label, file_names[i]);
            lv_obj_center(label);
            
            // Добавляем event handler для кнопки
            lv_obj_add_event_cb(btn, listbox_event_handler, LV_EVENT_CLICKED, NULL);
            
            // Сохраняем id кнопки (в user_data)
            lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        }
    }

    lvgl_port_unlock();
}



// Кнопка "Обновить"
static void update_button_event_handler(lv_event_t * e)
{
    ESP_LOGI(TAG, "🔄 Requesting file list...");
    lv_label_set_text(info_label, "🔄 Loading...");
    app_sd_card_info_start(); // Запускаем загрузку списка файлов
}

// Кнопка "Stop"
static void stop_button_event_handler(lv_event_t * e)
{
    ESP_LOGI(TAG, "🛑 Stop playback");
    audio_play_stop();
    lv_label_set_text(info_label, "Playback stopped");
}

// Создание экрана LVGL
static lv_obj_t *create_audio_player_screen(void)
{
    // Создаем screen
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

    // Header
    lv_obj_t *header = lv_label_create(screen);
    lv_label_set_text(header, "🎵 Audio Player");
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 10);

    // Listbox для файлов (теперь это просто container)
    list_obj = lv_obj_create(screen);
    lv_obj_set_size(list_obj, lv_pct(80), lv_pct(60));
    lv_obj_align(list_obj, LV_ALIGN_TOP_LEFT, 10, 40);
    lv_obj_set_flex_flow(list_obj, LV_FLEX_FLOW_COLUMN);

    // Info label
    info_label = lv_label_create(screen);
    lv_obj_set_width(info_label, lv_pct(80));
    lv_obj_align(info_label, LV_ALIGN_BOTTOM_LEFT, 10, -30);
    lv_label_set_text(info_label, "⏳ Loading file list...");

    // Кнопки управления
    lv_obj_t *btn_update = lv_btn_create(screen);
    lv_obj_set_size(btn_update, 70, 40);
    lv_obj_align(btn_update, LV_ALIGN_BOTTOM_RIGHT, -100, -10);

    lv_obj_t *label_update = lv_label_create(btn_update);
    lv_label_set_text(label_update, "update");

    lv_obj_t *btn_stop = lv_btn_create(screen);
    lv_obj_set_size(btn_stop, 70, 40);
    lv_obj_align(btn_stop, LV_ALIGN_BOTTOM_RIGHT, -20, -10);

    lv_obj_t *label_stop = lv_label_create(btn_stop);
    lv_label_set_text(label_stop, "stop");

    // Event handlers
    lv_obj_add_event_cb(btn_update, update_button_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_stop, stop_button_event_handler, LV_EVENT_CLICKED, NULL);

    return screen;
}

// Обработчик событий проекта
static void audio_player_event_handler(project_event_id_t id, const void *data, uint16_t len)
{
    switch (id) {
        case APP_EVENT_SD_FILE_LIST_LVGL:
            if (len >= sizeof(sd_file_list_lvgl_event_t)) {
                const sd_file_list_lvgl_event_t *msg = (const sd_file_list_lvgl_event_t*)data;
                handle_sd_file_list_event(msg);
            }
            break;
        case APP_EVENT_AUDIO_DONE_PLAY_FROM_FILE: {
            const audio_done_play_from_file_sys_msg_t *msg = (const audio_done_play_from_file_sys_msg_t*)data;
            lv_label_set_text_fmt(info_label, "✅ Completed: %s", msg->filename);
            break;
        }
        case APP_EVENT_AUDIO_STOPPED_PLAY_FILE_USER: {
            const audio_stopped_play_file_user_sys_msg_t *msg = (const audio_stopped_play_file_user_sys_msg_t*)data;
            lv_label_set_text_fmt(info_label, "🛑 Stopped: %s", msg->filename);
            break;
        }
        default:
            break;
    }
}

esp_err_t audio_player_app_init(void)
{
    // Регистрируем обработчик
    project_event_register_handler(APP_EVENT_SD_FILE_LIST_LVGL, audio_player_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_DONE_PLAY_FROM_FILE, audio_player_event_handler);
    project_event_register_handler(APP_EVENT_AUDIO_STOPPED_PLAY_FILE_USER, audio_player_event_handler);

    ESP_LOGI(TAG, "✅ Audio player app init");
    return ESP_OK;
}

void audio_player_app_start(void)
{
    lvgl_port_lock(0);
    lv_obj_t *screen = create_audio_player_screen();
    lv_scr_load(screen);
    lvgl_port_unlock();

    // Запрашиваем список файлов
    app_sd_card_info_start();
    ESP_LOGI(TAG, "▶️ Audio player started");
}

void audio_player_app_stop(void)
{
    ESP_LOGI(TAG, "⏹️ Audio player stopped");
}