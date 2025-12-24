/*
 * WiFi Credentials Header for Mira Station
 */

#ifndef WIFI_CREDENTIALS_H
#define WIFI_CREDENTIALS_H

#include <zephyr/net/net_if.h>

/* Function to set WiFi credentials */
int wifi_credentials_set(struct net_if *iface);

#endif /* WIFI_CREDENTIALS_H */
