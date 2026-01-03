#include "wifi_connect.h"
#include "common.h"
#include "station_gpio.h"
#include "station_sdcard.h"
#include "slot_uart.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sta, CONFIG_LOG_DEFAULT_LEVEL);




sd_data_t sd_data;
sys_data_t sys_data;

K_THREAD_DEFINE(uart_tid, 1024,slot_uart_thread,NULL, NULL, NULL,5, 0, 0);

/**
 * @brief アプリケーションのエントリーポイント
 *
 * システム起動後に最初に呼ばれる関数。
 * 本関数では、ステーション動作に必要な各種初期化処理のみを行う。
 *
 * 処理内容:
 *  1. GPIO 初期化
 *     - LED / 制御ピンなど、ボード依存GPIOの設定を行う
 *
 *  2. SDカード初期化
 *     - 設定データやログ保存に使用するSDカードを初期化する
 *     - 初期化結果は common.h で定義された sd_data 構造体に反映される
 *
 *  3. Wi-Fi 初期化
 *     - Wi-Fi ドライバおよびネットワークスタックを初期化し、
 *       アクセスポイントへの接続処理を開始する
 *
 * 本関数ではメインループやタスク生成は行わず、
 * 各機能の初期化後は処理を終了する。
 *
 * @return int
 *         常に 0 を返す（初期化エラーは各初期化関数内でログ出力される想定）
 */

int main(void)
{
    int ret = 0;

    init_gpio();

    init_sd_card();

    init_wifi();
    
    
    return ret;
}
