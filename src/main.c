

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sta, CONFIG_LOG_DEFAULT_LEVEL);

#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>
#include <zephyr/init.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/drivers/gpio.h>

#ifdef CONFIG_WIFI_READY_LIB
#include <net/wifi_ready.h>
#endif /* CONFIG_WIFI_READY_LIB */

#include "wifi_connect.h"


/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */


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

void init_gpio()
{
    gpio_pin_configure_dt(&coex_req, GPIO_OUTPUT_INACTIVE);     // OK
    gpio_pin_configure_dt(&coex_status0, GPIO_OUTPUT_INACTIVE); // OK

    gpio_pin_configure_dt(&coex_grant, GPIO_INPUT); // OK

    gpio_pin_configure_dt(&sw_ctrl0, GPIO_INPUT); // OK

    gpio_pin_configure_dt(&sw_ctrl1, GPIO_INPUT); // OK

    gpio_pin_configure_dt(&led_a, GPIO_OUTPUT_INACTIVE); // OK
    gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE); // OK

    gpio_pin_set_dt(&coex_req, 0);
    gpio_pin_set_dt(&coex_status0, 0);

    gpio_pin_set_dt(&led_a, 1);
    gpio_pin_set_dt(&led_b, 1);
}

int main(void)
{
    int ret = 0;

    init_gpio();

    init_wifi();
  
    return ret;
}
