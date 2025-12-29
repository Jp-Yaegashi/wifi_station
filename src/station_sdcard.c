#include <string.h>
#include "station_sdcard.h"
#include <string.h>
#include "common.h"

extern sd_data_t sd_data;

void init_sd_card(void)
{
    snprintf(sd_data.wifi_ssid, sizeof(sd_data.wifi_ssid), "%s", "huihui");
    snprintf(sd_data.wifi_passwd, sizeof(sd_data.wifi_passwd), "%s", "61121288b9");
    snprintf(sd_data.mcm_iot_hostname, sizeof(sd_data.mcm_iot_hostname), "%s", "api.stg-newsedtech-iot.onmira.cloud");
    snprintf(sd_data.endpoint_url[0], sizeof(sd_data.endpoint_url[0]), "%s", "/temperatures"); //温度アップロード
    snprintf(sd_data.endpoint_url[1], sizeof(sd_data.endpoint_url[1]), "%s", "/impacts"); //衝撃ログアップロード
}