#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/drivers/gpio.h>
#include <net/wifi_ready.h>
#include "wifi_connect.h"
#include "common.h"
#include "station_gpio.h"
#include "https_api.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_connect, LOG_LEVEL_INF);

#define WIFI_SHELL_MGMT_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | \
                                NET_EVENT_WIFI_DISCONNECT_RESULT)

#define STATUS_POLLING_MS 300

static struct net_mgmt_event_callback wifi_shell_mgmt_cb;
static struct net_mgmt_event_callback net_shell_mgmt_cb;

#ifdef CONFIG_WIFI_READY_LIB
static K_SEM_DEFINE(wifi_ready_state_changed_sem, 0, 1);
static bool wifi_ready_status;
#endif /* CONFIG_WIFI_READY_LIB */

static int register_wifi_ready(void);
static int cmd_wifi_status(void);
static void handle_wifi_connect_result(struct net_mgmt_event_callback *cb);
static void handle_wifi_disconnect_result(struct net_mgmt_event_callback *cb);
static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                    uint64_t mgmt_event, struct net_if *iface);
static int wifi_connect(void);
void wifi_ready_cb(bool wifi_ready);
#define CONFIG_STA_SAMPLE_START_WIFI_THREAD_STACK_SIZE 5200
#ifdef CONFIG_WIFI_READY_LIB
void start_wifi_thread(void);
#define THREAD_PRIORITY K_PRIO_COOP(CONFIG_NUM_COOP_PRIORITIES - 1)
K_THREAD_DEFINE(start_wifi_thread_id, CONFIG_STA_SAMPLE_START_WIFI_THREAD_STACK_SIZE,
                start_wifi_thread, NULL, NULL, NULL,
                THREAD_PRIORITY, 0, -1);
static struct
{
    const struct shell *sh;
    union
    {
        struct
        {
            uint8_t connected : 1;
            uint8_t connect_result : 1;
            uint8_t disconnect_requested : 1;
            uint8_t _unused : 5;
        };
        uint8_t all;
    };
} context;

extern sd_data_t sd_data;

/* Wi-Fi 機能全体の初期化を行う。
 * ネットワーク管理コールバックを登録し、
 * Wi-Fi Ready ライブラリ有効時は待機スレッドを起動する。
 */
int init_wifi(void)
{
    int ret = 0;
    net_mgmt_callback_init();

#ifdef CONFIG_WIFI_READY_LIB
    ret = register_wifi_ready();
    if (ret)
    {
        return ret;
    }
    k_thread_start(start_wifi_thread_id);
#else
    start_app();
#endif /* CONFIG_WIFI_READY_LIB */
    return 0;
}

/* 現在の Wi-Fi 接続状態を取得し、
 * SSID・RSSI・セキュリティ情報などをログ出力する。
 */
static int cmd_wifi_status(void)
{
    struct net_if *iface = net_if_get_default();
    struct wifi_iface_status status = {0};

    if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status,
                 sizeof(struct wifi_iface_status)))
    {
        LOG_INF("Status request failed");

        return -ENOEXEC;
    }

    LOG_INF("==================");
    LOG_INF("State: %s", wifi_state_txt(status.state));

    if (status.state >= WIFI_STATE_ASSOCIATED)
    {

        LOG_INF("Interface Mode: %s",
                wifi_mode_txt(status.iface_mode));
        LOG_INF("Link Mode: %s",
                wifi_link_mode_txt(status.link_mode));
        LOG_INF("SSID: %.32s", status.ssid);

        LOG_INF("Band: %s", wifi_band_txt(status.band));
        LOG_INF("Channel: %d", status.channel);
        LOG_INF("Security: %s", wifi_security_txt(status.security));
        LOG_INF("MFP: %s", wifi_mfp_txt(status.mfp));
        LOG_INF("RSSI: %d", status.rssi);
    }
    return 0;
}
/* Wi-Fi 接続結果イベントを処理する。
 * 接続成功・失敗を判定し、LED や内部状態を更新する。
 */
static void handle_wifi_connect_result(struct net_mgmt_event_callback *cb)
{
    const struct wifi_status *status =
        (const struct wifi_status *)cb->info;

    if (context.connected)
    {
        return;
    }

    if (status->status)
    {
        LOG_ERR("Connection failed (%d)", status->status);
        led_a_lights_down();
    }
    else
    {
        LOG_INF("Connected");
        context.connected = true;
        led_a_lights_up();
    }

    context.connect_result = true;
}

/* Wi-Fi 切断イベントを処理する。
 * 意図的切断か予期しない切断かを判定し、状態を更新する。
 */
static void handle_wifi_disconnect_result(struct net_mgmt_event_callback *cb)
{
    const struct wifi_status *status =
        (const struct wifi_status *)cb->info;

    if (!context.connected)
    {
        return;
    }

    if (context.disconnect_requested)
    {
        LOG_INF("Disconnection request %s (%d)",
                status->status ? "failed" : "done",
                status->status);
        context.disconnect_requested = false;
    }
    else
    {
        LOG_INF("Received Disconnected");
        context.connected = false;
    }

    cmd_wifi_status();
}

/* Wi-Fi 関連のネットワーク管理イベントを振り分ける
 * ディスパッチャ関数。
 */
static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                    uint64_t mgmt_event, struct net_if *iface)
{
    switch (mgmt_event)
    {
    case NET_EVENT_WIFI_CONNECT_RESULT:
        handle_wifi_connect_result(cb);
        break;
    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        handle_wifi_disconnect_result(cb);
        break;
    default:
        break;
    }
}

/* DHCP により IP アドレスが割り当てられた際の処理。
 * 割り当てられた IP を表示し、HTTPS 通信を開始する。
 */
static void print_dhcp_ip(struct net_mgmt_event_callback *cb)
{
    /* Get DHCP info from struct net_if_dhcpv4 and print */
    const struct net_if_dhcpv4 *dhcpv4 = cb->info;
    const struct in_addr *addr = &dhcpv4->requested_ip;
    char dhcp_info[128];

    net_addr_ntop(AF_INET, addr, dhcp_info, sizeof(dhcp_info));

    LOG_INF("DHCP IP address: %s", dhcp_info);

    https_post_json();
}

/* IPv4 DHCP などネットワーク層のイベントを処理する
 * 管理イベントハンドラ。
 */
static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                   uint64_t mgmt_event, struct net_if *iface)
{
    switch (mgmt_event)
    {
    case NET_EVENT_IPV4_DHCP_BOUND:
        print_dhcp_ip(cb);
        break;
    default:
        break;
    }
}

static struct wifi_connect_req_params wifi_params;

/* 設定された SSID / パスワードを使用して
 * Wi-Fi アクセスポイントへの接続を要求する。
 */
static int wifi_connect(void)
{
    LOG_INF("wifi_connect...................");
    struct net_if *iface = net_if_get_first_wifi();

    context.connected = false;
    context.connect_result = false;
    /* Initialize WiFi connection parameters completely */
    memset(&wifi_params, 0, sizeof(wifi_params));

    wifi_params.ssid = (uint8_t *)sd_data.wifi_ssid;
    wifi_params.ssid_length = strlen(sd_data.wifi_ssid);
    wifi_params.psk = (uint8_t *)sd_data.wifi_passwd;
    wifi_params.psk_length = strlen(sd_data.wifi_passwd);
    wifi_params.security = WIFI_SECURITY_TYPE_PSK; /* WPA2-PSK security */
    wifi_params.sae_password = NULL;
    wifi_params.sae_password_length = 0;
    wifi_params.channel = WIFI_CHANNEL_ANY;
    wifi_params.band = WIFI_FREQ_BAND_2_4_GHZ; /* Explicit 2.4GHz for Mimas2 */
    wifi_params.mfp = WIFI_MFP_DISABLE;        /* Disable MFP for compatibility */
    wifi_params.timeout = SYS_FOREVER_MS;      /* Let the driver handle timeouts */

    if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params, sizeof(wifi_params)))
    {
        LOG_ERR("Connection request failed");

        return -ENOEXEC;
    }

    LOG_INF("Connection requested");

    return 0;
}

/* Wi-Fi 接続制御のメイン処理。
 * Wi-Fi Ready 状態を監視しながら AP へ接続し、
 * 接続完了後は状態監視を継続する。
 */
int start_app(void)
{

    while (1)
    {
#ifdef CONFIG_WIFI_READY_LIB
        int ret;

        LOG_INF("Waiting for Wi-Fi to be ready");
        ret = k_sem_take(&wifi_ready_state_changed_sem, K_FOREVER);
        if (ret)
        {
            LOG_ERR("Failed to take semaphore: %d", ret);
            return ret;
        }

    check_wifi_ready:
        if (!wifi_ready_status)
        {
            LOG_INF("Wi-Fi is not ready");
            /* Perform any cleanup and stop using Wi-Fi and wait for
             * Wi-Fi to be ready
             */
            continue;
        }
#endif /* CONFIG_WIFI_READY_LIB */
        wifi_connect();

        while (!context.connect_result)
        {
            cmd_wifi_status();
            k_sleep(K_MSEC(STATUS_POLLING_MS));
        }

        if (context.connected)
        {
            cmd_wifi_status();
#ifdef CONFIG_WIFI_READY_LIB
            ret = k_sem_take(&wifi_ready_state_changed_sem, K_FOREVER);
            if (ret)
            {
                LOG_ERR("Failed to take semaphore: %d", ret);
                return ret;
            }
            goto check_wifi_ready;
#else
            k_sleep(K_FOREVER);
#endif /* CONFIG_WIFI_READY_LIB */
        }
    }

    return 0;
}

/* Wi-Fi 制御用スレッドのエントリポイント。
 * start_app() をスレッドコンテキストで実行する。
 */
void start_wifi_thread(void)
{
    start_app();
}

/* Wi-Fi Ready 状態変更時に呼ばれるコールバック。
 * Ready / Not Ready を記録し、待機中の処理を解除する。
 */
void wifi_ready_cb(bool wifi_ready)
{
    LOG_DBG("Is Wi-Fi ready?: %s", wifi_ready ? "yes" : "no");
    wifi_ready_status = wifi_ready;
    k_sem_give(&wifi_ready_state_changed_sem);
}
#endif /* CONFIG_WIFI_READY_LIB */

/* ネットワーク管理イベント（Wi-Fi / DHCP）の
 * コールバック初期化および登録を行う。
 */
void net_mgmt_callback_init(void)
{
    memset(&context, 0, sizeof(context));

    net_mgmt_init_event_callback(&wifi_shell_mgmt_cb,
                                 wifi_mgmt_event_handler,
                                 WIFI_SHELL_MGMT_EVENTS);

    net_mgmt_add_event_callback(&wifi_shell_mgmt_cb);

    net_mgmt_init_event_callback(&net_shell_mgmt_cb,
                                 net_mgmt_event_handler,
                                 NET_EVENT_IPV4_DHCP_BOUND);

    net_mgmt_add_event_callback(&net_shell_mgmt_cb);

    LOG_INF("Starting %s with CPU frequency: %d MHz", CONFIG_BOARD, SystemCoreClock / MHZ(1));
    k_sleep(K_SECONDS(1));
}

#ifdef CONFIG_WIFI_READY_LIB

/* Wi-Fi Ready ライブラリへコールバックを登録する。
 * Wi-Fi デバイスの利用可能状態変化を検知するために使用。
 */
static int register_wifi_ready(void)
{
    int ret = 0;
    wifi_ready_callback_t cb;
    struct net_if *iface = net_if_get_first_wifi();

    if (!iface)
    {
        LOG_ERR("Failed to get Wi-Fi interface");
        return -1;
    }

    cb.wifi_ready_cb = wifi_ready_cb;

    LOG_DBG("Registering Wi-Fi ready callbacks");
    ret = register_wifi_ready_callback(cb, iface);
    if (ret)
    {
        LOG_ERR("Failed to register Wi-Fi ready callbacks %s", strerror(ret));
        return ret;
    }

    return ret;
}
#endif /* CONFIG_WIFI_READY_LIB */