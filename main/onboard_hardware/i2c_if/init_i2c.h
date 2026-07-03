#ifndef INIT_I2C_H

#define INIT_I2C_H
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
//#include "config.h"
//#include "init_modules.h"

extern i2c_master_bus_handle_t i2c_bus_handle;

esp_err_t bsp_init_i2c(void);
void bsp_deinit_i2c(void);

#endif // INIT_I2C_H