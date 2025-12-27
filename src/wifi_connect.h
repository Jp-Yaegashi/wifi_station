
#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include <zephyr/net/net_if.h>
int init_wifi(void);
void net_mgmt_callback_init(void);
#endif /* WIFI_CONNECT_H */