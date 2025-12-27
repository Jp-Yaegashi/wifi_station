
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>
#include <nrfx_clock.h>
#include <zephyr/device.h>
#include <zephyr/net/net_config.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/init.h>
#include "wifi_connect.h"



#define THREAD_PRIORITY K_PRIO_COOP(CONFIG_NUM_COOP_PRIORITIES - 1)
LOG_MODULE_REGISTER(wifi_station, CONFIG_LOG_DEFAULT_LEVEL);

#define CONFIG_STA_SAMPLE_START_WIFI_THREAD_STACK_SIZE 5200

K_THREAD_DEFINE(start_wifi_thread_id, CONFIG_STA_SAMPLE_START_WIFI_THREAD_STACK_SIZE,
                start_wifi_thread, NULL, NULL, NULL,
                THREAD_PRIORITY, 0, -1);


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
    printk("start main\n");
    int ret = 0;

    init_gpio();

    ret = init_wifi();

    if (ret)
    {
        return ret;
    }
   

    return 0;
}