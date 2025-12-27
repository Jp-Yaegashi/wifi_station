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
int init_wifi();
void start_wifi_thread(void);
int start_app(void);
static int wifi_connect(void);

static void handle_wifi_connect_result(struct net_mgmt_event_callback *cb);
static void handle_wifi_disconnect_result(struct net_mgmt_event_callback *cb);
int wifi_connect(void);
static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                   uint64_t mgmt_event, struct net_if *iface);
static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                    uint64_t mgmt_event, struct net_if *iface);
int wifi_credentials_set(struct net_if *iface);
                                    
static int wm02c_interface_recovery(struct net_if *iface);
static int rpu_scan_recovery(struct net_if *iface);
static int wm02c_power_cycle(void);

#endif /* WIFI_CONNECT_H */