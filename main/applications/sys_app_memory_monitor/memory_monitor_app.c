// main/applications/sys_app_memory_monitor/memory_monitor_app.c

#include "memory_monitor_app.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "project_event_handler_manager/project_event_handler_manager.h"

static const char *TAG = "APP_MEM_MON";

static void monitor_task(void *pvParameters)
{
    uint32_t period_ms = (uint32_t)(pvParameters);
    if (period_ms == 0) period_ms = 10000; // 10 сек — по умолчанию

    while (1) {
        size_t free_dram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        size_t min_dram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        size_t largest_dram = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

        size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t min_psram = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
        size_t largest_psram = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

        UBaseType_t stack_left = uxTaskGetStackHighWaterMark(NULL);

        // ✅ Отправляем событие
        memory_status_sys_msg_t msg = {
            .free_dram_kb = free_dram / 1024,
            .min_dram_kb = min_dram / 1024,
            .largest_dram_kb = largest_dram / 1024,
            .free_psram_mb = free_psram / (1024 * 1024),
            .min_psram_mb = min_psram / (1024 * 1024),
            .largest_psram_mb = largest_psram / (1024 * 1024),
            .stack_words = stack_left
        };
        project_event_send(APP_EVENT_MEMORY_STATUS, &msg, sizeof(msg));

        // 📝 only log DRAM if critical (PSRAM=32MB, no need to warn on <50MB)
        if (free_dram < 50 * 1024) {
            ESP_LOGW(TAG, "⚠️ Low DRAM: %u KiB free", (unsigned)(free_dram / 1024));
        }

        vTaskDelay(pdMS_TO_TICKS(period_ms));
    }
}

esp_err_t app_memory_monitor_init(void)
{
    ESP_LOGI(TAG, "Memory monitor init");
    return ESP_OK;
}

void app_memory_monitor_start(uint32_t period_sec)
{
    uint32_t period_ms = period_sec * 1000;
    xTaskCreate(monitor_task, "app_mem_mon_task", 4096, (void*)period_ms, 1, NULL);
}

void app_memory_monitor_deinit(void) {}