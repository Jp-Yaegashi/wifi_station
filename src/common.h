#include <stdint.h> 
#include <stdio.h>
typedef struct {
    char mcm_iot_hostname[256]; //APIホストサーバー
    char endpoint_url[5][128]; //エンドポイントURL
    char mcm_login_id[128];
    char mcm_login_passwd[128];
    char wifi_ssid[64];
    char wifi_passwd[128];
    uint8_t wifi_authentication; //WiFi認証方式
} sd_data_t;