/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 * @brief WiFi shell sample main function
 */

#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>
#include <nrfx_clock.h>
#include <zephyr/device.h>
#include <zephyr/net/net_config.h>
#include <zephyr/drivers/gpio.h>
/*
static const struct gpio_dt_spec coex_status0 =
    GPIO_DT_SPEC_GET(DT_NODELABEL(coex_status0), gpios);

static const struct gpio_dt_spec coex_req =
    GPIO_DT_SPEC_GET(DT_NODELABEL(coex_req), gpios);

static const struct gpio_dt_spec coex_grant =
    GPIO_DT_SPEC_GET(DT_NODELABEL(coex_grant), gpios);

static const struct gpio_dt_spec sw_ctrl0 =
    GPIO_DT_SPEC_GET(DT_NODELABEL(sw_ctrl0), gpios);

static const struct gpio_dt_spec sw_ctrl1 =
    GPIO_DT_SPEC_GET(DT_NODELABEL(sw_ctrl1), gpios);

static const struct gpio_dt_spec led_a =
    GPIO_DT_SPEC_GET(DT_NODELABEL(led_a), gpios);
static const struct gpio_dt_spec led_b =
    GPIO_DT_SPEC_GET(DT_NODELABEL(led_b), gpios);
*/
int main(void)
{
	printk("Boot start\n");
	/* ① 内部 DC/DC */


	printk("Starting %s with CPU frequency: %d MHz\n", CONFIG_BOARD, SystemCoreClock/MHZ(1));
	/*
	gpio_pin_configure_dt(&coex_req, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&coex_status0, GPIO_OUTPUT_INACTIVE); //OK
     
    gpio_pin_configure_dt(&coex_grant, GPIO_OUTPUT_INACTIVE); //OK

    gpio_pin_configure_dt(&sw_ctrl0, GPIO_OUTPUT_INACTIVE);//OK
    
    gpio_pin_configure_dt(&sw_ctrl1, GPIO_OUTPUT_INACTIVE);//OK


    gpio_pin_configure_dt(&led_a, GPIO_OUTPUT_INACTIVE);//OK
    gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE);//OK

	gpio_pin_set_dt(&coex_req, 0);  
	gpio_pin_set_dt(&coex_status0, 0);  
	gpio_pin_set_dt(&coex_grant, 0);  
	gpio_pin_set_dt(&sw_ctrl0, 0);  
	gpio_pin_set_dt(&sw_ctrl1, 0);  
	gpio_pin_set_dt(&led_a, 1);  
	gpio_pin_set_dt(&led_b, 1);


*/

	return 0;
}