#ifndef SYS_APPLICATIONS_H

#define SYS_APPLICATIONS_H

#include "sys_app_rcp_c6_update/rcp_c6_update_app.h"

#include "sys_app_memory_monitor/memory_monitor_app.h"

#include "sys_app_time_sync/time_sync_app.h"

#include "sys_app_sd_card_info/sd_card_info_app.h"

#include "sys_app_wifi_manager/wifi_manager_app.h"

#include "esp_err.h"


// hear you can setup autostart all apps
esp_err_t sys_applications_init(void);
#endif