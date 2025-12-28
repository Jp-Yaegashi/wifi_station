
#include <zephyr/drivers/gpio.h>
#include "station_gpio.h"

const struct gpio_dt_spec coex_status0 =
    GPIO_DT_SPEC_GET(DT_NODELABEL(coex_status0), gpios);

const struct gpio_dt_spec coex_req =
    GPIO_DT_SPEC_GET(DT_NODELABEL(coex_req), gpios);

const struct gpio_dt_spec coex_grant =
    GPIO_DT_SPEC_GET(DT_NODELABEL(coex_grant), gpios);

const struct gpio_dt_spec sw_ctrl0 =
    GPIO_DT_SPEC_GET(DT_NODELABEL(sw_ctrl0), gpios);

const struct gpio_dt_spec sw_ctrl1 =
    GPIO_DT_SPEC_GET(DT_NODELABEL(sw_ctrl1), gpios);

const struct gpio_dt_spec led_a =
    GPIO_DT_SPEC_GET(DT_NODELABEL(led_a), gpios);
const struct gpio_dt_spec led_b =
    GPIO_DT_SPEC_GET(DT_NODELABEL(led_b), gpios);

void init_gpio()
{
    gpio_pin_configure_dt(&coex_req, GPIO_OUTPUT_INACTIVE);     
    gpio_pin_configure_dt(&coex_status0, GPIO_OUTPUT_INACTIVE); 

    gpio_pin_configure_dt(&coex_grant, GPIO_INPUT); 

    gpio_pin_configure_dt(&sw_ctrl0, GPIO_INPUT); 

    gpio_pin_configure_dt(&sw_ctrl1, GPIO_INPUT); 

    gpio_pin_configure_dt(&led_a, GPIO_OUTPUT_INACTIVE); 
    gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE); 

    gpio_pin_set_dt(&coex_req, 0);
    gpio_pin_set_dt(&coex_status0, 0);

    gpio_pin_set_dt(&led_a, 0);
    gpio_pin_set_dt(&led_b, 0);
}

void led_a_lights_up(void){
    gpio_pin_set_dt(&led_a, 1);
}
void led_a_lights_down(void){
    gpio_pin_set_dt(&led_a, 0);
}