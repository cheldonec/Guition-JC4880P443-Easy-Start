#include "init_netif.h"
#include "esp_log.h"

static const char *TAG = "init_netif";

esp_netif_t *sta_netif = NULL;
//esp_wifi_handle_t wifi_handle = NULL;

esp_err_t init_netif_STA(void)
{
    esp_err_t ret = ESP_OK;
    esp_netif_init();
    sta_netif = esp_netif_create_default_wifi_sta();
    if (!sta_netif) {
        ESP_LOGE(TAG, "❌ Failed to create default Wi-Fi STA");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Wi-Fi STA Interface created");

    
    return ret;
}

void deinit_netif_STA(void)
{
    if (sta_netif) {
        esp_netif_destroy_default_wifi(sta_netif);
        sta_netif = NULL;
    }
}