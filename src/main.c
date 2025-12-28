

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sta, CONFIG_LOG_DEFAULT_LEVEL);

#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>
#include <zephyr/init.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/drivers/gpio.h>
#include <net/wifi_ready.h>
#include "wifi_connect.h"
#include "common.h"
#include "station_gpio.h"
#include "station_sdcard.h"


/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */




sd_data_t sd_data;


int main(void)
{
    int ret = 0;

    init_gpio();

    init_sd_card();

    init_wifi();
  
    return ret;
}
