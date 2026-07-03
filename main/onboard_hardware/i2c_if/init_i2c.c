#include "init_i2c.h"
#include "board_config.h"

static const char *TAG = "init_i2c_module";

// --- Инициализация шины I2C ---
i2c_master_bus_handle_t i2c_bus_handle = NULL;

esp_err_t bsp_init_i2c(void)
{
    esp_err_t ret = ESP_FAIL;
    ESP_LOGI(TAG, "Initializing I2C...");
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = 0,
        .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
        .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags.enable_internal_pullup = true,
    };
    ret = i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed");
    } else {
        ESP_LOGI(TAG, "I2C init success");
    }
    return ret;
}

void bsp_deinit_i2c(void)
{
    
    if (i2c_bus_handle) {
        ESP_LOGI(TAG, "Deinitializing I2C...");
        i2c_del_master_bus(i2c_bus_handle);
        i2c_bus_handle = NULL;
    }
}