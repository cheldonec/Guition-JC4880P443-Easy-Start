// main/applications/wifi_manager/wifi_manager_app.h
#ifndef WIFI_MANAGER_APP_H
#define WIFI_MANAGER_APP_H

#include "esp_err.h"
#include "esp_wifi.h"

bool app_wifi_is_sta_connected(void);
// Инициализация
esp_err_t app_wifi_init(void);

void wifi_scan_networks(void);

// Учётные данные
// busket = 0 это последнее удачное соединение
// busket 1 - 10 предпочитаемы сети в приоритете от 1 до 10
esp_err_t wifi_set_STA_credentials(uint8_t busket_1_10, const char *ssid, const char *password);
esp_err_t wifi_load_STA_credentials(uint8_t busket_1_10, char *ssid_out, size_t ssid_len, char *pass_out, size_t pass_len);
esp_err_t app_wifi_start_sta(esp_netif_t *sta_netif, const char *reserv_ssid, const char *reserv_pass);

void app_wifi_deinit(void);

#endif // WIFI_MANAGER_APP_H