// main/project_event_handler/project_event_handler.c
#include "project_event_handler_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
//#include "heap_caps.h"

static const char *TAG = "PROJECT_EVT";

#define EVENT_QUEUE_SIZE 32
static QueueHandle_t s_event_queue = NULL;
static TaskHandle_t s_event_task = NULL;


static struct {
    project_event_id_t id;
    project_event_cb_t cb;
} s_handlers[MAX_HANDLERS] = {0};

// --- Internal: найти свободный слот или существующий handler ---
static int find_handler_slot(project_event_id_t id, project_event_cb_t cb, bool find_free)
{
    for (int i = 0; i < MAX_HANDLERS; i++) {
        if (find_free && s_handlers[i].cb == NULL) return i;
        if (!find_free && s_handlers[i].id == id && s_handlers[i].cb == cb) return i;
    }
    return -1;
}

// --- Public API ---
esp_err_t project_event_init(void)
{
    if (s_event_queue) return ESP_ERR_INVALID_STATE;

    s_event_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(project_event_t));
    if (!s_event_queue) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return ESP_ERR_NO_MEM;
    }

    // Задача обработчика событий (высокий приоритет — важные события)
    xTaskCreate(project_event_handler, "project_event_handler", 4096, NULL, 6, &s_event_task);
    ESP_LOGI(TAG, "Event handler initialized (queue_size=%d, task_priority=6)", EVENT_QUEUE_SIZE);
    return ESP_OK;
}

esp_err_t project_event_send(project_event_id_t id, const void *data, uint16_t data_len)
{
    if (!s_event_queue) {
        ESP_LOGE(TAG, "Event handler not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    void *copy = NULL;
    
    // 🔧 ИСПРАВЛЕНИЕ: не выделять память, если данные отсутствуют
    if (data && data_len > 0) {
        copy = heap_caps_malloc(data_len, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
        if (!copy) {
            ESP_LOGE(TAG, "Failed to allocate %u bytes for event data", data_len);
            return ESP_ERR_NO_MEM;
        }
        memcpy(copy, data, data_len);
    } else if (data_len > 0) {
        // data == NULL, но data_len > 0 — это ошибка (нельзя копировать NULL)
        ESP_LOGE(TAG, "Invalid event: data_len=%u but data=NULL", data_len);
        return ESP_ERR_INVALID_ARG;
    }

    project_event_t evt = {
        .id = id,
        .data = copy,
        .data_len = data_len
    };

    if (xQueueSend(s_event_queue, &evt, pdMS_TO_TICKS(100)) != pdTRUE) {
        if (copy) heap_caps_free(copy); // откат, если очередь полна
        ESP_LOGW(TAG, "Event queue full, dropping event %04X", id);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t project_event_register_handler(project_event_id_t id, project_event_cb_t cb)
{
    if (!cb) return ESP_ERR_INVALID_ARG;

    int slot = find_handler_slot(id, cb, true);
    if (slot < 0) {
        ESP_LOGE(TAG, "No free handler slot for event %04X", id);
        return ESP_ERR_NO_MEM;
    }

    s_handlers[slot].id = id;
    s_handlers[slot].cb = cb;
    ESP_LOGI(TAG, "Registered handler for event %04X", id);
    return ESP_OK;
}

esp_err_t project_event_unregister_handler(project_event_id_t id, project_event_cb_t cb)
{
    int slot = find_handler_slot(id, cb, false);
    if (slot < 0) {
        ESP_LOGW(TAG, "Handler for event %04X not found", id);
        return ESP_ERR_NOT_FOUND;
    }

    s_handlers[slot].cb = NULL; // не очищаем id — для безопасности
    ESP_LOGI(TAG, "Unregistered handler for event %04X", id);
    return ESP_OK;
}

// --- Main task: берёт события из очереди и dispatch-ит ---
void project_event_handler(void *arg)
{
    project_event_t evt;

    for (;;) {
        if (xQueueReceive(s_event_queue, &evt, portMAX_DELAY) == pdTRUE) {
            // dispatch на registered handlers
            for (int i = 0; i < MAX_HANDLERS; i++) {
                if (s_handlers[i].cb && s_handlers[i].id == evt.id) {
                    ESP_LOGD(TAG, "Dispatching %04X → %p", evt.id, s_handlers[i].cb);
                    s_handlers[i].cb(evt.id, evt.data, evt.data_len);
                }
            }

            // Free allocated data
            if (evt.data) {
                heap_caps_free(evt.data);
            }
        }
    }
}