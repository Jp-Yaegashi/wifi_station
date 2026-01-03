#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/util.h>

#define RX_RING_SIZE 512
#define LINE_MAX     128

static const struct device *uart_slot = DEVICE_DT_GET(DT_NODELABEL(uart1));

RING_BUF_DECLARE(rx_ring, RX_RING_SIZE);
K_SEM_DEFINE(rx_sem, 0, 1);

void slot_puts(const char *s)
{
    while (*s) {
        uart_poll_out(uart_slot, (unsigned char)*s++);
    }
}

/* UART IRQ: 受信したbyteをring bufferへ */
static void uart1_cb(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            uint8_t tmp[32];
            int n = uart_fifo_read(dev, tmp, sizeof(tmp));
            if (n > 0) {
                /* ringに詰める（溢れた分は捨てる） */
                uint32_t wrote = ring_buf_put(&rx_ring, tmp, (uint32_t)n);
                (void)wrote;
                k_sem_give(&rx_sem);
            }
        }
    }
}

/* 監視スレッド：ringから取り出して、行単位で表示 */
void slot_uart_thread(void)
{
    if (!device_is_ready(uart_slot)) {
        printk("*** uart1 not ready ***\n");
        return;
    }

    uart_irq_callback_user_data_set(uart_slot, uart1_cb, NULL);
    uart_irq_rx_enable(uart_slot);

    printk("*** uart1 monitor start ***\n");

    char line[LINE_MAX];
    int  line_len = 0;

    while (1) {
        /* 受信が来るまで待つ（常時監視） */
        k_sem_take(&rx_sem, K_FOREVER);

        /* ringに溜まってる分を全部吐き出す */
        uint8_t c;
        while (ring_buf_get(&rx_ring, &c, 1) == 1) {

            /* CR/LFで1行確定 */
            if (c == '\r' || c == '\n') {
                if (line_len > 0) {
                    line[line_len] = '\0';
                    printk("SLOT RX: %s\n", line);
                    line_len = 0;
                }
                continue;
            }

            /* 行バッファに追加（溢れたら強制確定） */
            if (line_len < (LINE_MAX - 1)) {
                line[line_len++] = (char)c;
            } else {
                line[line_len] = '\0';
                printk("UART1 RX (trunc): %s\n", line);
                line_len = 0;
            }
        }
    }
}
