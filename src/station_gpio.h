#ifndef STATION_GPIO_H
#define STATION_GPIO_H


#include <zephyr/drivers/gpio.h>

void init_gpio();
void led_a_lights_up(void);
void led_a_lights_down(void);
void set_enable_slot_uart(uint8_t slot);
void mcp23s17_isr(const struct device *dev,
                  struct gpio_callback *cb,
                  uint32_t pins);

#endif /* STATION_GPIO_H */