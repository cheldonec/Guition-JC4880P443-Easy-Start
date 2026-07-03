// applications_template.c
#include "applications_template_not_compile.h"
#include "applications_template.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "APP_TEMPLATE";

// Глобальные переменные приложения
// static SomeHandle s_handle;

esp_err_t app_template_init(void)
{
    ESP_LOGI(TAG, "Initializing...");
    // → Инициализация ресурсов: GPIO, LVGL-элементы, подключение к Wi-Fi и т.п.
    // → Нельзя использовать `vTaskDelay` здесь — только синхронные операции
    ESP_LOGI(TAG, "Initialized OK");
    return ESP_OK;
}

void app_template_start(void)
{
    // Запуск в отдельной задаче, если приложение должно работать параллельно
    // xTaskCreate(app_template_task, "...", stack, ..., priority, ...);
    xTaskCreate(app_template_task, "app_template_task", 8192, NULL, 5, NULL);
}

static void app_template_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Running...");
    // → Бесконечный цикл или обработка событий
    while (1) {
        // ...
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_template_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing...");
    // → Очистка ресурсов: закрытие файлов, деструкция handle'ов, отписка от событий
    // → Вызывается только после остановки задач
    ESP_LOGI(TAG, "Deinitialized OK");
}