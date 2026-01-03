
#include <zephyr/drivers/gpio.h>
#include "station_gpio.h"
#include "common.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(station_gpio, LOG_LEVEL_INF);

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

const struct gpio_dt_spec exp_spi_reset =
    GPIO_DT_SPEC_GET(DT_NODELABEL(exp_spi_reset), gpios);

const struct gpio_dt_spec unit_spi_reset =
    GPIO_DT_SPEC_GET(DT_NODELABEL(unit_spi_reset), gpios);

const struct gpio_dt_spec slot_uart_enable =
    GPIO_DT_SPEC_GET(DT_NODELABEL(slot_uart_enable), gpios); // UART 通信有効・無効

const struct gpio_dt_spec slot_detect =
    GPIO_DT_SPEC_GET(DT_NODELABEL(slot_detect), gpios); // SLOT 挿入検出用

/* SPI接続GPIOエキスパンダ
 * - mcp17: スロット検出・電源制御（MCP23S17：16bit）
 * - mcp08: UART切替SEL制御（MCP23S08：8bit）
 */
const struct device *mcp17 = DEVICE_DT_GET(DT_NODELABEL(mcp23s17)); // slot 制御用
const struct device *mcp08 = DEVICE_DT_GET(DT_NODELABEL(mcp23s08)); // UART制御用

static struct gpio_callback mcp_cb;

/* 割り込み処理用のワーク（チャタリング対策で遅延実行） */
static struct k_work_delayable gpio_isr_work;

uint8_t slot_state = 0;

void print_u8_bin(uint8_t v);

void print_u8_bin(uint8_t v)
{
    printk("\n*** print_u8_bin ***\n");
    for (int i = 7; i >= 0; i--)
    {
        printk("%d", (v >> i) & 1);
    }
    printk("\n");
}

/**
 * @brief 割り込みハンドラから呼ばれるワーク関数
 *
 * 割り込みコンテキスト外でGPIOを読み取る
 */
static void gpio_isr_work_handler(struct k_work *work)
{
    printk("\n*** GPIO interrupt work handler ***\n");

    for (int in_pin = 0; in_pin < 8; in_pin++)
    {
        int v = gpio_pin_get(mcp17, in_pin); /* GPA0..7 */

        if (v < 0)
        {
            printk("mcp17 read error pin%d: %d\n", in_pin, v);
            continue;
        }
        if (v == LOW)
        {
            printk("Insert the card into slot %d\n", in_pin);
            gpio_pin_set(mcp17, in_pin + 8, HIGT); // カード充電電源ON
            set_enable_slot_uart(in_pin);
            slot_state |= (1U << in_pin);
        }
        else
        {
            printk("No card in slot %d\n", in_pin);
            gpio_pin_set(mcp17, in_pin + 8, LOW); // カード充電電源OFF
            slot_state &= ~(1U << in_pin);        // 0にする
        }
    }

    /* 処理完了後、MCP23S17の全ピン割り込みを再有効化 */
    for (int pin = 0; pin < 8; pin++)
    {
        gpio_pin_interrupt_configure(mcp17, pin, GPIO_INT_EDGE_BOTH);
    }
    if(slot_state==0x00){ //どのスロットにもカード刺してない場合、通信無効設定
        set_enable_slot_uart(0xff);
    }
    //print_u8_bin(slot_state);

    //printk("Interrupts re-enabled\n");
}

/**
 * @brief MCP23S17 のGPIO入力変化（スロット挿入検出）を処理する割り込みハンドラ
 *
 * MCP23S17 側の入力ピン（想定：GPA0～GPA7）が変化したときに呼ばれ、
 * 対応するスロットの電源制御出力（想定：GPB0～GPB7 = ピン8～15）をON/OFFします。
 *
 * - pins には「どのピンが割り込み要因か」を示すビットマスクが渡る想定です。
 *   ※Zephyrのgpioコールバック仕様では pins はビットマスクで来るため、
 *   本実装の pins==0/1/2... 判定は将来的に BIT(n) 判定へ変更推奨です。
 *
 * @param dev  割り込み元GPIOデバイス（未使用）
 * @param cb   登録したコールバック（未使用）
 * @param pins 変化したピンのビットマスク（例：BIT(0) がGPA0）
 */
void mcp23s17_isr(const struct device *dev,
                  struct gpio_callback *cb,
                  uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    /* チャタリング対策：変化したピンの割り込みを無効化 */
    for (int pin = 0; pin < 8; pin++)
    {
        if (pins & BIT(pin))
        {
            gpio_pin_interrupt_configure(dev, pin, GPIO_INT_DISABLE);
        }
    }

    /* 500ms後にワークを実行（割り込み再有効化含む） */
    k_work_reschedule(&gpio_isr_work, K_MSEC(500));
}
/**
 * @brief Station基板のGPIO初期化（SoC直結GPIO + MCP23S17/MCP23S08 の初期化）
 *
 * この関数は以下を行います：
 * 1) WM02C/COEX関連GPIOの方向設定（req/status0出力、grant入力）
 * 2) スイッチ入力（sw_ctrl0/sw_ctrl1）設定
 * 3) LED（led_a/led_b）を出力として初期化
 * 4) MCP23S17/MCP23S08のRESET（または有効化）信号を有効化し、通信可能になるまで待機
 * 5) MCP23S08：UART切替用SELピン（0～2）を出力に設定
 * 6) MCP23S17：スロット検出入力（0～7）を入力＋割り込み有効、電源制御出力（8～15）を出力に設定
 * 7) 初期状態として各種出力をOFF、必要に応じてテスト設定でON
 *
 * 注意：
 * - MCPの通信有効化後に待ち時間(k_msleep)を入れているのは、
 *   リセット解除直後の安定化およびSPIバス立ち上がりのためです。
 */
void init_gpio()
{
    printk("### init_gpio start ###\n");

    int ret;
    // WM02C モジュール
    gpio_pin_configure_dt(&coex_req, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&coex_status0, GPIO_OUTPUT_INACTIVE);

    gpio_pin_configure_dt(&coex_grant, GPIO_INPUT);

    gpio_pin_configure_dt(&sw_ctrl0, GPIO_INPUT);

    gpio_pin_configure_dt(&sw_ctrl1, GPIO_INPUT);

    // LED 制御
    gpio_pin_configure_dt(&led_a, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE);

    // SPI制御
    gpio_pin_configure_dt(&exp_spi_reset, GPIO_OUTPUT_INACTIVE);  // シリアル通信切り替え用
    gpio_pin_configure_dt(&unit_spi_reset, GPIO_OUTPUT_INACTIVE); // スロット検出・電源制御用

    // uart制御
    gpio_pin_configure_dt(&slot_uart_enable, GPIO_OUTPUT_INACTIVE);

    gpio_pin_set_dt(&exp_spi_reset, HIGT);  // uart 通信有効
    gpio_pin_set_dt(&unit_spi_reset, HIGT); // slot SPI通信有効
    k_msleep(1);                            // MCP23S08T通信有効後、待ち時間必須

    // MCP23S08T初期化(出力設定)
    gpio_pin_configure(mcp08, MCP23S08_GP0, GPIO_OUTPUT);
    gpio_pin_configure(mcp08, MCP23S08_GP1, GPIO_OUTPUT);
    gpio_pin_configure(mcp08, MCP23S08_GP2, GPIO_OUTPUT);

    // MCP23S17T初期化
    // スロット挿入検出(入力設定)

    printk("Configuring MCP23S17 pins and interrupts...\n");
    for (int pin = 0; pin < 8; pin++)
    {
        gpio_pin_configure(mcp17, pin, GPIO_INPUT | GPIO_PULL_UP);
        ret = gpio_pin_interrupt_configure(mcp17, pin, GPIO_INT_EDGE_BOTH);
        if (ret != 0)
        {
            printk("Failed to configure interrupt for pin %d: %d\n", pin, ret);
        }
    }

    /* MCP23S17のピンに直接コールバック登録（全ピン0-7） */
    gpio_init_callback(&mcp_cb, mcp23s17_isr, 0xFF); /* 下位8ビット全て */
    ret = gpio_add_callback(mcp17, &mcp_cb);
    if (ret != 0)
    {
        printk("Failed to add callback: %d\n", ret);
        return;
    }

    printk("MCP23S17 interrupt initialized on device %p\n", mcp17);

    // 初期状態確認
    printk("=== MCP23S17 GPA0-7 Initial Status ===\n");

    // 電源制御(出力設定)
    /* GPB0..7 を出力に（電源制御など） */
    for (int pin = 8; pin < 16; pin++)
    {
        gpio_pin_configure(mcp17, pin, GPIO_OUTPUT_INACTIVE);
        gpio_pin_set(mcp17, pin, LOW); // 全て電源出力OFF設定
    }

    for (int pin = 0; pin < 8; pin++)
    {
        int val = gpio_pin_get(mcp17, pin);
        printk("GPA%d: %d\n", pin, val);
        if (val == LOW)
        {
            slot_state |= (1U << pin);
            gpio_pin_set(mcp17, pin + 8, HIGT); // UART電源設定 ON
        }
        else
        {
            slot_state &= ~(1U << pin);        // 0にする
            gpio_pin_set(mcp17, pin + 8, LOW); // UART電源設定 OFF
        }
       
    }

    if (slot_state & SLOT_IN_0)
    {
        set_enable_slot_uart(0);
    }
    else if (slot_state & SLOT_IN_1)
    {
        set_enable_slot_uart(1);
    }
    else if (slot_state & SLOT_IN_2)
    {
        set_enable_slot_uart(2);
    }
    else if (slot_state & SLOT_IN_3)
    {
        set_enable_slot_uart(3);
    }
    else if (slot_state & SLOT_IN_4)
    {
        set_enable_slot_uart(4);
    }
    else if (slot_state & SLOT_IN_5)
    {
        set_enable_slot_uart(5);
    }
    else if (slot_state & SLOT_IN_6)
    {
        set_enable_slot_uart(6);
    }
    else if (slot_state & SLOT_IN_7)
    {
        set_enable_slot_uart(7);
    }else{ //どのスロットにもカード刺してない場合、通信無効設定
        set_enable_slot_uart(0xff);
    }

    //print_u8_bin(slot_state);

    gpio_pin_set_dt(&coex_req, LOW);
    gpio_pin_set_dt(&coex_status0, LOW);

    gpio_pin_set_dt(&led_a, LOW);
    gpio_pin_set_dt(&led_b, LOW);

  
    // 割り込み処理用ワークの初期化（遅延実行可能）
    k_work_init_delayable(&gpio_isr_work, gpio_isr_work_handler);
}
/**
 * @brief Status LED(A) を点灯する
 *
 * led_a を High にして点灯します（Active High前提）。
 */
void led_a_lights_up(void)
{
    gpio_pin_set_dt(&led_a, HIGT);
}
/**
 * @brief Status LED(A) を消灯する
 *
 * led_a を Low にして消灯します（Active High前提）。
 */
void led_a_lights_down(void)
{
    gpio_pin_set_dt(&led_a, LOW);
}
/**
 * @brief WiFi LED(B) を点灯する
 *
 * led_b を High にして点灯します（Active High前提）。
 */
void led_b_lights_up(void)
{
    gpio_pin_set_dt(&led_b, HIGT);
}
/**
 * @brief WiFi LED(B) を消灯する
 *
 * led_b を Low にして消灯します（Active High前提）。
 */
void led_b_lights_down(void)
{
    gpio_pin_set_dt(&led_b, LOW);
}

/**
 * @brief スロットUART通信の有効/無効と、UART切替スロット番号の設定
 *
 * MCP23S08 の 3本のSEL信号（SEL0/SEL1/SEL2）を設定して、
 * 0～7 のスロットを選択します。
 *
 * - slot: 0～7 で該当スロットを選択し UART を有効化します。
 * - slot: 0xFF の場合は UART を無効化します（全スロット切り離し）。
 *
 * 注意：
 * - slot_uart_enable は Active Low（0=有効、1=無効）という前提の実装です。
 *
 * @param slot スロット番号（0～7）、または 0xFF（UART無効）
 */
void set_enable_slot_uart(uint8_t slot)
{
    printk("set_enable_slot_uart: slot = %d\n", slot);
    if (slot == 0)
    {
        gpio_pin_set_dt(&slot_uart_enable, LOW); // UART通信有効
        gpio_pin_set(mcp08, MCP23S08_GP0, LOW);  // SEL0
        gpio_pin_set(mcp08, MCP23S08_GP1, LOW);  // SEL1
        gpio_pin_set(mcp08, MCP23S08_GP2, LOW);  // SEL2
    }
    else if (slot == 1)
    {
        gpio_pin_set_dt(&slot_uart_enable, LOW); // UART通信有効
        gpio_pin_set(mcp08, MCP23S08_GP0, HIGT); // SEL0
        gpio_pin_set(mcp08, MCP23S08_GP1, LOW);  // SEL1
        gpio_pin_set(mcp08, MCP23S08_GP2, LOW);  // SEL2
    }
    else if (slot == 2)
    {
        gpio_pin_set_dt(&slot_uart_enable, LOW); // UART通信有効
        gpio_pin_set(mcp08, MCP23S08_GP0, LOW);  // SEL0
        gpio_pin_set(mcp08, MCP23S08_GP1, HIGT); // SEL1
        gpio_pin_set(mcp08, MCP23S08_GP2, LOW);  // SEL2
    }
    else if (slot == 3)
    {
        gpio_pin_set_dt(&slot_uart_enable, LOW); // UART通信有効
        gpio_pin_set(mcp08, MCP23S08_GP0, HIGT); // SEL0
        gpio_pin_set(mcp08, MCP23S08_GP1, HIGT); // SEL1
        gpio_pin_set(mcp08, MCP23S08_GP2, LOW);  // SEL2
    }
    else if (slot == 4)
    {
        gpio_pin_set_dt(&slot_uart_enable, LOW); // UART通信有効
        gpio_pin_set(mcp08, MCP23S08_GP0, LOW);  // SEL0
        gpio_pin_set(mcp08, MCP23S08_GP1, LOW);  // SEL1
        gpio_pin_set(mcp08, MCP23S08_GP2, HIGT); // SEL2
    }
    else if (slot == 5)
    {
        gpio_pin_set_dt(&slot_uart_enable, LOW); // UART通信有効
        gpio_pin_set(mcp08, MCP23S08_GP0, HIGT); // SEL0
        gpio_pin_set(mcp08, MCP23S08_GP1, LOW);  // SEL1
        gpio_pin_set(mcp08, MCP23S08_GP2, HIGT); // SEL2
    }
    else if (slot == 6)
    {
        gpio_pin_set_dt(&slot_uart_enable, LOW); // UART通信有効
        gpio_pin_set(mcp08, MCP23S08_GP0, LOW);  // SEL0
        gpio_pin_set(mcp08, MCP23S08_GP1, HIGT); // SEL1
        gpio_pin_set(mcp08, MCP23S08_GP2, HIGT); // SEL2
    }
    else if (slot == 7)
    {
        gpio_pin_set_dt(&slot_uart_enable, LOW); // UART通信有効
        gpio_pin_set(mcp08, MCP23S08_GP0, HIGT); // SEL0
        gpio_pin_set(mcp08, MCP23S08_GP1, HIGT); // SEL1
        gpio_pin_set(mcp08, MCP23S08_GP2, HIGT); // SEL2
    }
    else if (slot == 0xff)
    {                                             // 全て無効
        gpio_pin_set_dt(&slot_uart_enable, HIGT); // UART通信無効
    }
}