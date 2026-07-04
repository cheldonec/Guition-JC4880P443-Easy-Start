// sd_card_io.c
#include "sd_card_io.h"
#include "board_config.h"
#include "esp_vfs_fat.h"


#include "esp_log.h"
#include "sd_pwr_ctrl.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "project_event_handler_manager/project_event_handler_manager.h" 
//#include "config.h"

static const char *TAG = "SD_CARD_IO";

// Глобальная карта — деинициализируется при deinit()
sdmmc_card_t *sd_card = NULL;

esp_err_t bsp_sd_card_io_init(void)
{
    ESP_LOGD(TAG, "Mounting SD card (Slot 0, GPIO14–19, LDO4)...");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = 4,
    };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;
    esp_err_t ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LDO4 (%d)", ret);
        return ret;
    }
    
    host.pwr_ctrl_handle = pwr_ctrl_handle;

    const sdmmc_slot_config_t slot_config = {
        .cd = SDMMC_SLOT_NO_CD,
        .wp = SDMMC_SLOT_NO_WP,
        .width = 4,
        .flags = 0,
    };

    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 2,
        .allocation_unit_size = 16 * 1024,
    };

    ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &sd_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD (%d)", ret);
        sd_pwr_ctrl_del_on_chip_ldo(pwr_ctrl_handle);
        return ret;
    }

    ESP_LOGD(TAG, "SD card (Slot 0) mounted!");

    // ✅ Отправляем событие — только если монтирование УСПЕШНО
    sd_card_mounted_sys_msg_t msg = { .mount_point = "/sdcard" };
    project_event_send(APP_EVENT_SD_CARD_MOUNTED, &msg, sizeof(msg));

    return ESP_OK;
}

void bsp_sd_card_io_deinit(void)
{
    if (sd_card) {
        ESP_LOGD(TAG, "Unmounting SD card...");

        // ✅ Отправляем событие до размонтирования
        project_event_send(APP_EVENT_SD_CARD_UNMOUNTED, NULL, 0);

        esp_vfs_fat_sdcard_unmount("/sdcard", sd_card);
        sd_card = NULL;
    }
}
