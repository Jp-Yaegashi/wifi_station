/*
 * WiFi  Header for Mira Station
 */

#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include <zephyr/net/net_if.h>

/* WiFi credentials - configured for Mira Station */
#define WIFI_SSID "huihui"
#define WIFI_PSK "61121288b9"


void net_mgmt_callback_init(void);
int init_wifi(void);
void start_wifi_thread(void);
int start_app(void);



int wifi_connect(void);


int wifi_credentials_set(struct net_if *iface);
                                    

#endif /* WIFI_CONNECT_H */