
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

const struct gpio_dt_spec exp_spi_reset =
    GPIO_DT_SPEC_GET(DT_NODELABEL(exp_spi_reset), gpios);

const struct gpio_dt_spec unit_spi_reset =
    GPIO_DT_SPEC_GET(DT_NODELABEL(unit_spi_reset), gpios);

const struct gpio_dt_spec slot_uart_enable =
    GPIO_DT_SPEC_GET(DT_NODELABEL(slot_uart_enable), gpios); //UART 通信有効・無効

/* SPI接続GPIOエキスパンダ
 * - mcp17: スロット検出・電源制御（MCP23S17：16bit）
 * - mcp08: UART切替SEL制御（MCP23S08：8bit）
 */
const struct device *mcp17 = DEVICE_DT_GET(DT_NODELABEL(mcp23s17)); //slot 制御用
const struct device *mcp08 = DEVICE_DT_GET(DT_NODELABEL(mcp23s08)); //UART制御用

static struct gpio_callback mcp_cb;

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

    /* 割り込みが来たら、入力を読む */
     int val = gpio_pin_get(mcp17, pins); /* GPA0 */
    printk("INT: GPA0=%d\n", val);

    if(pins==0){
        if(val==1){
            gpio_pin_set(mcp17,8, 1); //電源ON
        }else{
            gpio_pin_set(mcp17,8, 0);//電源OFF
        }
    }else if(pins==1){
        if(val==1){
            gpio_pin_set(mcp17,9, 1); //電源ON
        }else{
            gpio_pin_set(mcp17,9, 0);//電源OFF
        }
    }else if(pins==2){
        if(val==1){
            gpio_pin_set(mcp17,10, 1); //電源ON
        }else{
            gpio_pin_set(mcp17,10, 0);//電源OFF
        }
    }else if(pins==3){
        if(val==1){
            gpio_pin_set(mcp17,11, 1); //電源ON
        }else{
            gpio_pin_set(mcp17,11, 0);//電源OFF
        }
    }else if(pins==4){
        if(val==1){
            gpio_pin_set(mcp17,12, 1); //電源ON
        }else{
            gpio_pin_set(mcp17,12, 0);//電源OFF
        }
    }else if(pins==5){
        if(val==1){
            gpio_pin_set(mcp17,13, 1); //電源ON
        }else{
            gpio_pin_set(mcp17,13, 0);//電源OFF
        }
    }else if(pins==6){
        if(val==1){
            gpio_pin_set(mcp17,14, 1); //電源ON
        }else{
            gpio_pin_set(mcp17,14, 0);//電源OFF
        }
    }else if(pins==7){
        if(val==1){
            gpio_pin_set(mcp17,15, 1); //電源ON
        }else{
            gpio_pin_set(mcp17,15, 0);//電源OFF
        }
    }
   
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
    //WM02C モジュール
    gpio_pin_configure_dt(&coex_req, GPIO_OUTPUT_INACTIVE);     
    gpio_pin_configure_dt(&coex_status0, GPIO_OUTPUT_INACTIVE); 

    gpio_pin_configure_dt(&coex_grant, GPIO_INPUT); 

    gpio_pin_configure_dt(&sw_ctrl0, GPIO_INPUT); 

    gpio_pin_configure_dt(&sw_ctrl1, GPIO_INPUT); 

    //LED 制御
    gpio_pin_configure_dt(&led_a, GPIO_OUTPUT_INACTIVE); 
    gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE); 

    //SPI制御
    gpio_pin_configure_dt(&exp_spi_reset, GPIO_OUTPUT_INACTIVE);  //シリアル通信切り替え用
    gpio_pin_configure_dt(&unit_spi_reset, GPIO_OUTPUT_INACTIVE);  //スロット検出・電源制御用

    //uart制御
    gpio_pin_configure_dt(&slot_uart_enable, GPIO_OUTPUT_INACTIVE); 

    gpio_pin_set_dt(&exp_spi_reset, 1); //uart 通信有効
    gpio_pin_set_dt(&unit_spi_reset, 1); //slot SPI通信有効
    k_msleep(1);//MCP23S08T通信有効後、待ち時間必須
 
   
    //MCP23S08T初期化(出力設定)
    gpio_pin_configure(mcp08, 0, GPIO_OUTPUT);
    gpio_pin_configure(mcp08, 1, GPIO_OUTPUT);
    gpio_pin_configure(mcp08, 2, GPIO_OUTPUT);


    //MCP23S17T初期化
    //スロット挿入検出(入力設定)
    gpio_pin_configure(mcp17, 0, GPIO_INPUT);
    gpio_pin_configure(mcp17, 1, GPIO_INPUT);
    gpio_pin_configure(mcp17, 2, GPIO_INPUT);
    gpio_pin_configure(mcp17, 3, GPIO_INPUT);
    gpio_pin_configure(mcp17, 4, GPIO_INPUT);
    gpio_pin_configure(mcp17, 5, GPIO_INPUT);
    gpio_pin_configure(mcp17, 6, GPIO_INPUT);
    gpio_pin_configure(mcp17, 7, GPIO_INPUT);
    /* 割り込み条件（エッジ） */
    gpio_pin_interrupt_configure(mcp17, 0, GPIO_INT_EDGE_BOTH);
    gpio_pin_interrupt_configure(mcp17, 1, GPIO_INT_EDGE_BOTH);
    gpio_pin_interrupt_configure(mcp17, 2, GPIO_INT_EDGE_BOTH);
    gpio_pin_interrupt_configure(mcp17, 3, GPIO_INT_EDGE_BOTH);
    gpio_pin_interrupt_configure(mcp17, 4, GPIO_INT_EDGE_BOTH);
    gpio_pin_interrupt_configure(mcp17, 5, GPIO_INT_EDGE_BOTH);
    gpio_pin_interrupt_configure(mcp17, 6, GPIO_INT_EDGE_BOTH);
    gpio_pin_interrupt_configure(mcp17, 7, GPIO_INT_EDGE_BOTH);

    /* コールバック登録 */
    gpio_init_callback(&mcp_cb, mcp23s17_isr, BIT(0));
    gpio_add_callback(mcp17, &mcp_cb);

    gpio_init_callback(&mcp_cb, mcp23s17_isr, BIT(1));
    gpio_add_callback(mcp17, &mcp_cb);

    gpio_init_callback(&mcp_cb, mcp23s17_isr, BIT(2));
    gpio_add_callback(mcp17, &mcp_cb);

    gpio_init_callback(&mcp_cb, mcp23s17_isr, BIT(3));
    gpio_add_callback(mcp17, &mcp_cb);

    gpio_init_callback(&mcp_cb, mcp23s17_isr, BIT(4));
    gpio_add_callback(mcp17, &mcp_cb);

    gpio_init_callback(&mcp_cb, mcp23s17_isr, BIT(5));
    gpio_add_callback(mcp17, &mcp_cb);

    gpio_init_callback(&mcp_cb, mcp23s17_isr, BIT(6));
    gpio_add_callback(mcp17, &mcp_cb);

    gpio_init_callback(&mcp_cb, mcp23s17_isr, BIT(7));
    gpio_add_callback(mcp17, &mcp_cb);

    //電源制御(出力設定) 
    gpio_pin_configure(mcp17, 8, GPIO_OUTPUT);
    gpio_pin_configure(mcp17, 9, GPIO_OUTPUT);
    gpio_pin_configure(mcp17, 10, GPIO_OUTPUT);
    gpio_pin_configure(mcp17, 11, GPIO_OUTPUT);
    gpio_pin_configure(mcp17, 12, GPIO_OUTPUT);
    gpio_pin_configure(mcp17, 13, GPIO_OUTPUT);
    gpio_pin_configure(mcp17, 14, GPIO_OUTPUT);
    gpio_pin_configure(mcp17, 15, GPIO_OUTPUT);

    //全て電源出力OFF設定
    gpio_pin_set(mcp17,8, 0);
    gpio_pin_set(mcp17,9, 0);
    gpio_pin_set(mcp17,10, 0);
    gpio_pin_set(mcp17,11, 0);
    gpio_pin_set(mcp17,12, 0);
    gpio_pin_set(mcp17,13, 0);
    gpio_pin_set(mcp17,14, 0);
    gpio_pin_set(mcp17,15, 0);


    gpio_pin_set_dt(&coex_req, 0);
    gpio_pin_set_dt(&coex_status0, 0);

    gpio_pin_set_dt(&led_a, 0);
    gpio_pin_set_dt(&led_b, 0);

        
    //テスト　設定start
    gpio_pin_set(mcp08,0, 0); //SEL0
    gpio_pin_set(mcp08,1, 1); //SEL1
    gpio_pin_set(mcp08,2, 0); //SEL2

    gpio_pin_set(mcp17,8, 1);
    gpio_pin_set(mcp17,9, 1);
    gpio_pin_set(mcp17,10, 1);
    gpio_pin_set(mcp17,11, 1);
    gpio_pin_set(mcp17,12, 1);
    gpio_pin_set(mcp17,13, 1);
    gpio_pin_set(mcp17,14, 1);
    gpio_pin_set(mcp17,15, 1);
    //テスト　設定 end

    printk("### init_gpio end ###\n");

}
/**
 * @brief Status LED(A) を点灯する
 *
 * led_a を High にして点灯します（Active High前提）。
 */
void led_a_lights_up(void){
    gpio_pin_set_dt(&led_a, 1);
}
/**
 * @brief Status LED(A) を消灯する
 *
 * led_a を Low にして消灯します（Active High前提）。
 */
void led_a_lights_down(void){
    gpio_pin_set_dt(&led_a, 0);
}
/**
 * @brief WiFi LED(B) を点灯する
 *
 * led_b を High にして点灯します（Active High前提）。
 */
void led_b_lights_up(void){
    gpio_pin_set_dt(&led_b, 1);
}
/**
 * @brief WiFi LED(B) を消灯する
 *
 * led_b を Low にして消灯します（Active High前提）。
 */
void led_b_lights_down(void){
    gpio_pin_set_dt(&led_b, 0);
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
    if(slot==0){
        gpio_pin_set_dt(&slot_uart_enable, 0); //UART通信有効
        gpio_pin_set(mcp08,0, 0); //SEL0
        gpio_pin_set(mcp08,1, 0); //SEL1
        gpio_pin_set(mcp08,2, 0); //SEL2
    }else if(slot==1){
        gpio_pin_set_dt(&slot_uart_enable, 0); //UART通信有効
        gpio_pin_set(mcp08,0, 1); //SEL0
        gpio_pin_set(mcp08,1, 0); //SEL1
        gpio_pin_set(mcp08,2, 0); //SEL2
    }else if(slot==2){
        gpio_pin_set_dt(&slot_uart_enable, 0); //UART通信有効
        gpio_pin_set(mcp08,0, 0); //SEL0
        gpio_pin_set(mcp08,1, 1); //SEL1
        gpio_pin_set(mcp08,2, 0); //SEL2
    }else if(slot==3){
        gpio_pin_set_dt(&slot_uart_enable, 0); //UART通信有効
        gpio_pin_set(mcp08,0, 1); //SEL0
        gpio_pin_set(mcp08,1, 1); //SEL1
        gpio_pin_set(mcp08,2, 0); //SEL2        
    }else if(slot==4){
        gpio_pin_set_dt(&slot_uart_enable, 0); //UART通信有効
        gpio_pin_set(mcp08,0, 0); //SEL0
        gpio_pin_set(mcp08,1, 0); //SEL1
        gpio_pin_set(mcp08,2, 1); //SEL2
        
    }else if(slot==5){
        gpio_pin_set_dt(&slot_uart_enable, 0); //UART通信有効
        gpio_pin_set(mcp08,0, 1); //SEL0
        gpio_pin_set(mcp08,1, 0); //SEL1
        gpio_pin_set(mcp08,2, 1); //SEL2
        
    }else if(slot==6){
        gpio_pin_set_dt(&slot_uart_enable, 0); //UART通信有効
        gpio_pin_set(mcp08,0, 0); //SEL0
        gpio_pin_set(mcp08,1, 1); //SEL1
        gpio_pin_set(mcp08,2, 1); //SEL2
        
    }else if(slot==7){
        gpio_pin_set_dt(&slot_uart_enable, 0); //UART通信有効
        gpio_pin_set(mcp08,0, 1); //SEL0
        gpio_pin_set(mcp08,1, 1); //SEL1
        gpio_pin_set(mcp08,2, 1); //SEL2        
    }else if(slot==0xff){ //全て無効
        gpio_pin_set_dt(&slot_uart_enable, 1); //UART通信無効
    }

}