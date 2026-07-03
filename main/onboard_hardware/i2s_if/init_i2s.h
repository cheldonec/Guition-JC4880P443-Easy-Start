#ifndef INIT_I2S_H

#define INIT_I2S_H

#include "esp_err.h"
#include "driver/i2s_std.h"

extern i2s_chan_handle_t i2s_tx_handle;
extern i2s_chan_handle_t i2s_rx_handle;

esp_err_t bsp_init_i2s(void);
void bsp_deinit_i2s(void);

#endif // INIT_I2S_H