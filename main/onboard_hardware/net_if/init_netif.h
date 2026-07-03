#ifndef INIT_NETIF_H

#define INIT_NETIF_H

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_err.h"

extern esp_netif_t *sta_netif;
//extern esp_wifi_handle_t wifi_handle;

esp_err_t init_netif_STA(void);
void deinit_netif_STA(void);

#endif