/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/dhcpv4.h>

#include "wifi_station.h"

LOG_MODULE_REGISTER(wifi_manager, LOG_LEVEL_INF);

static struct net_if *wifi_iface;
static struct net_mgmt_event_callback wifi_mgmt_cb;
static struct net_mgmt_event_callback net_mgmt_cb;

static bool connected = false;
static K_SEM_DEFINE(wifi_connected, 0, 1);

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint32_t mgmt_event, struct net_if *iface)
{
	const struct wifi_status *status = (const struct wifi_status *)cb->info;

	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		if (status->status) {
			LOG_ERR("Connection failed (%d)", status->status);
			current_state = APP_STATE_WIFI_DISCONNECTED;
		} else {
			LOG_INF("Connected to WiFi");
			connected = true;
			k_sem_give(&wifi_connected);
		}
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		if (connected) {
			LOG_INF("Disconnected from WiFi");
			connected = false;
			current_state = APP_STATE_WIFI_DISCONNECTED;
		}
		break;
	default:
		break;
	}
}

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				   uint32_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_IPV4_ADDR_ADD:
		LOG_INF("IPv4 address added");
		current_state = APP_STATE_WIFI_CONNECTED;
		break;
	case NET_EVENT_IPV4_ADDR_DEL:
		LOG_INF("IPv4 address removed");
		if (connected) {
			current_state = APP_STATE_WIFI_DISCONNECTED;
		}
		break;
	default:
		break;
	}
}

int wifi_manager_init(void)
{
	/* Wait for WiFi interface to be available */
	k_sleep(K_SECONDS(2));
	
	/* Get WiFi interface with retry logic */
	wifi_iface = net_if_get_first_wifi();
	if (!wifi_iface) {
		LOG_WRN("No WiFi interface found on first try, retrying...");
		
		/* Wait for interfaces to be initialized */
		k_sleep(K_SECONDS(1));
		wifi_iface = net_if_get_first_wifi();
		
		if (!wifi_iface) {
			LOG_ERR("No WiFi interface found - checking all interfaces");
			struct net_if *iface;
			int count = 0;
			
			STRUCT_SECTION_FOREACH(net_if, iface) {
				LOG_INF("Interface %d: %p, device: %s", 
					count++, iface, 
					net_if_get_device(iface)->name);
			}
			return -ENODEV;
		}
	}

	/* Setup event callbacks */
	net_mgmt_init_event_callback(&wifi_mgmt_cb, wifi_mgmt_event_handler,
				     NET_EVENT_WIFI_CONNECT_RESULT |
				     NET_EVENT_WIFI_DISCONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_mgmt_cb);

	net_mgmt_init_event_callback(&net_mgmt_cb, net_mgmt_event_handler,
				     NET_EVENT_IPV4_ADDR_ADD |
				     NET_EVENT_IPV4_ADDR_DEL);
	net_mgmt_add_event_callback(&net_mgmt_cb);

	LOG_INF("WiFi manager initialized");
	return 0;
}

/* Static WiFi connection parameters to avoid stack issues */
static struct wifi_connect_req_params wifi_mgr_params;

int wifi_connect(void)
{
	if (!wifi_iface) {
		LOG_ERR("WiFi interface not available");
		return -ENODEV;
	}

	/* Initialize WiFi parameters completely */
	memset(&wifi_mgr_params, 0, sizeof(wifi_mgr_params));
	
	/* Set WiFi parameters */
	wifi_mgr_params.ssid = WIFI_SSID;
	wifi_mgr_params.ssid_length = strlen(WIFI_SSID);
	
	/* Configure for open network (no PSK) */
	wifi_mgr_params.psk = NULL;
	wifi_mgr_params.psk_length = 0;
	wifi_mgr_params.security = WIFI_SECURITY_TYPE_PSK;
	wifi_mgr_params.channel = WIFI_CHANNEL_ANY;
	wifi_mgr_params.band = WIFI_FREQ_BAND_2_4_GHZ;
	wifi_mgr_params.mfp = WIFI_MFP_DISABLE;
	wifi_mgr_params.timeout = SYS_FOREVER_MS;

	LOG_INF("Connecting to WiFi SSID: %s", WIFI_SSID);

	if (net_mgmt(NET_REQUEST_WIFI_CONNECT, wifi_iface,
		     &wifi_mgr_params, sizeof(wifi_mgr_params))) {
		LOG_ERR("WiFi connection request failed");
		return -EIO;
	}

	/* Wait for connection with timeout */
	if (k_sem_take(&wifi_connected, K_SECONDS(30)) != 0) {
		LOG_ERR("WiFi connection timeout");
		return -ETIMEDOUT;
	}

	/* Start DHCP client */
	net_dhcpv4_start(wifi_iface);

	return 0;
}

int wifi_disconnect(void)
{
	if (!wifi_iface) {
		return -ENODEV;
	}

	if (net_mgmt(NET_REQUEST_WIFI_DISCONNECT, wifi_iface, NULL, 0)) {
		LOG_ERR("WiFi disconnect request failed");
		return -EIO;
	}

	connected = false;
	return 0;
}

bool wifi_is_connected(void)
{
	return connected && (current_state == APP_STATE_WIFI_CONNECTED);
}
