#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>


static const struct gpio_dt_spec wifi_buck_en =
    GPIO_DT_SPEC_GET(DT_NODELABEL(wifi_buck_en), gpios);

static const struct gpio_dt_spec wifi_en =
    GPIO_DT_SPEC_GET(DT_NODELABEL(wifi_en), gpios);

/* 起動時に自動実行される */
static int wifi_power_init(void)
{

	printk("### wifi_power_init ###\n");
    
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
    k_msleep(1000);

    return 0;
}

/* ★ ここが最重要 ★
 * Wi-Fi ドライバより先に実行させる
 */
SYS_INIT(wifi_power_init, POST_KERNEL, 0);
