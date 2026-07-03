// sd_card_io.h
#ifndef SD_CARD_IO_H
#define SD_CARD_IO_H

#include "esp_err.h"
#include "driver/sdmmc_host.h"


// Глобальная переменная — карта, для последующей деинициализации
//extern sdmmc_card_t *sd_card;
extern sdmmc_card_t *sd_card;
// Инициализация и деинициализация
esp_err_t bsp_sd_card_io_init(void);
void bsp_sd_card_io_deinit(void);

#endif // SD_CARD_IO_H