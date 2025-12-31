#include <stdint.h> 
#include <stdio.h>
typedef struct {
    char mcm_iot_hostname[256]; //APIホストサーバー
    char endpoint_url[5][64]; //エンドポイントURL
    char mcm_login_id[128];
    char mcm_login_passwd[128];
    char wifi_ssid[64];
    char wifi_passwd[128];
    uint8_t wifi_authentication; //WiFi認証方式
} sd_data_t;

typedef struct {
    char token[512]; //トークン   
} sys_data_t;

#define     HIGT            1
#define     LOW             0
#define     MCP23S08_GP0    0
#define     MCP23S08_GP1    1
#define     MCP23S08_GP2    2
#define     HTTPS_PORT      "443"
#define     CA_CERT_TAG     1