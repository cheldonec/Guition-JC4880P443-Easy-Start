// main/applications/sys_app_memory_monitor/memory_monitor_app.h

#ifndef MEMORY_MONITOR_APP_H
#define MEMORY_MONITOR_APP_H

#include "esp_err.h"

/**
 * @brief Инициализация монитора памяти
 */
esp_err_t app_memory_monitor_init(void);

/**
 * @brief Запуск задачи мониторинга памяти
 * @param period_sec Период опроса (в секундах), например, 10
 *
 * @details
 * - Создаёт задачу `monitor_task`
 * - Отправляет событие `APP_EVENT_MEMORY_STATUS` каждый период
 * - Если `period_sec == 0` — будет использовано значение по умолчанию (10 сек)
 */
void app_memory_monitor_start(uint32_t period_sec);

/**
 * @brief Деинициализация монитора памяти
 */
void app_memory_monitor_deinit(void);

#endif