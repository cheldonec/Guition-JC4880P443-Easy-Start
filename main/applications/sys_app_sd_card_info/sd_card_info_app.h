#ifndef SD_CARD_INFO_APP_H
#define SD_CARD_INFO_APP_H

#include "esp_err.h"

esp_err_t app_sd_card_info_init(void);
void app_sd_card_info_start(void);
void app_sd_card_info_deinit(void);

#endif // SD_CARD_INFO_APP_H