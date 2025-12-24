/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WIFI_STATION_H_
#define WIFI_STATION_H_

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* WiFi credentials - configured for Mira Station */
#define WIFI_SSID "huihui"
#define WIFI_PSK "61121288b9"

/* Application states */
enum app_state {
	APP_STATE_INIT,
	APP_STATE_WIFI_CONNECTING,
	APP_STATE_WIFI_CONNECTED,
	APP_STATE_WIFI_DISCONNECTED,
	APP_STATE_ERROR
};

/* Global application state */
extern enum app_state current_state;

/* Function prototypes */
int wifi_manager_init(void);
/* Note: wifi_connect() is implemented as static function in main.c */
int wifi_disconnect(void);
bool wifi_is_connected(void);

int uart_shell_init(void);
int test_shell_init(void);
int led_control_init(void);
void led_set_state(int led_id, bool on);
void led_set_pattern(int pattern);

int ble_service_init(void);

/* LED test functions */
int led_test_init(void);
void led_test_start(void);
void led_test_stop(void);
bool led_test_is_active(void);

/* Display test functions */
int display_test_init(void);
int display_test_run_full_test(void);
int display_test_show_color(uint16_t color);
int display_test_set_backlight(uint8_t brightness);

#endif /* WIFI_STATION_H_ */
