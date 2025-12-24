#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>


#include <zephyr/sys/printk.h>


/* DeviceTree から GPIO を取得 */
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
    GPIO_DT_SPEC_GET(DT_NODELABEL(led_b), gpios);*/


static const struct gpio_dt_spec wifi_buck_en =
    GPIO_DT_SPEC_GET(DT_NODELABEL(wifi_buck_en), gpios);

static const struct gpio_dt_spec wifi_en =
    GPIO_DT_SPEC_GET(DT_NODELABEL(wifi_en), gpios);

/* 起動時に自動実行される */
static int wifi_power_init(void)
{

	printk("### wifi_power_init ###\n");
    /*

    if (!device_is_ready(coex_status0.port) ||
        !device_is_ready(coex_req.port) ||
        !device_is_ready(coex_grant.port)  ||
        !device_is_ready(sw_ctrl0.port)  ||
        !device_is_ready(sw_ctrl1.port)

    ) {
        return -ENODEV;
    }

    gpio_pin_configure(coex_req.port, 11, GPIO_INPUT | GPIO_PULL_DOWN);
    gpio_pin_configure_dt(&coex_status0, GPIO_OUTPUT_INACTIVE); //OK
    
   
    
    gpio_pin_configure_dt(&coex_grant, GPIO_OUTPUT_INACTIVE); //OK

    gpio_pin_configure_dt(&sw_ctrl0, GPIO_OUTPUT_INACTIVE);//OK
    
    gpio_pin_configure_dt(&sw_ctrl1, GPIO_OUTPUT_INACTIVE);//OK


    gpio_pin_configure_dt(&led_a, GPIO_OUTPUT_INACTIVE);//OK
    gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE);//OK




    gpio_pin_set_dt(&coex_status0, 0); //0K
    gpio_pin_set_dt(&coex_req, 0);     //ng
  
    gpio_pin_set_dt(&coex_grant, 1);     //
    gpio_pin_set_dt(&sw_ctrl0, 1);  //OK
    gpio_pin_set_dt(&sw_ctrl1, 1);   //OK


    gpio_pin_set_dt(&led_a, 1);  //OK
    gpio_pin_set_dt(&led_b, 1);  //OK

    printk("raw=%d\n", gpio_pin_get_raw(coex_req.port, 11));

    
*/
if (!device_is_ready(wifi_buck_en.port) ||
        !device_is_ready(wifi_en.port)) {
        return -ENODEV;
    }

    gpio_pin_configure_dt(&wifi_buck_en, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&wifi_en, GPIO_OUTPUT_INACTIVE);
	k_msleep(60);
    gpio_pin_set_dt(&wifi_buck_en, 0); /* BUCK_EN */
    k_msleep(100);

    printk("### wifi_en called ###\n");
    gpio_pin_set_dt(&wifi_en, 0);      /* IOVDD */
    k_msleep(100);

    /* WM02C 推奨電源シーケンス */
    printk("### wifi_buck_en called ###\n");
    gpio_pin_set_dt(&wifi_buck_en, 1); /* BUCK_EN */
    k_msleep(1000);

    printk("### wifi_en called ###\n");
    gpio_pin_set_dt(&wifi_en, 1);      /* IOVDD */
    k_msleep(5000);

    return 0;
}

/* ★ ここが最重要 ★
 * Wi-Fi ドライバより先に実行させる
 */
SYS_INIT(wifi_power_init, POST_KERNEL, 0);
