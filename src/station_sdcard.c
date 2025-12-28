#include <string.h>
#include "station_sdcard.h"
#include <string.h>
#include "common.h"

extern sd_data_t sd_data;

void init_sd_card(void)
{
    snprintf(sd_data.wifi_ssid, sizeof(sd_data.wifi_ssid), "%s", "huihui");
    snprintf(sd_data.wifi_passwd, sizeof(sd_data.wifi_passwd), "%s", "61121288b9");
}