

#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>
#include <zephyr/init.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/drivers/gpio.h>

#ifdef CONFIG_WIFI_READY_LIB
#include <net/wifi_ready.h>
#endif /* CONFIG_WIFI_READY_LIB */

#include "wifi_connect.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_connect, LOG_LEVEL_INF);

#define WIFI_SHELL_MODULE "wifi"

#define WIFI_SHELL_MGMT_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | \
                                NET_EVENT_WIFI_DISCONNECT_RESULT)

#define MAX_SSID_LEN 32
#define STATUS_POLLING_MS 300

/* 1000 msec = 1 sec */
#define LED_SLEEP_TIME_MS 100

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

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
    }
    else
    {
        LOG_INF("Connected");
        context.connected = true;
    }

    context.connect_result = true;
}

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

static void print_dhcp_ip(struct net_mgmt_event_callback *cb)
{
    /* Get DHCP info from struct net_if_dhcpv4 and print */
    const struct net_if_dhcpv4 *dhcpv4 = cb->info;
    const struct in_addr *addr = &dhcpv4->requested_ip;
    char dhcp_info[128];

    net_addr_ntop(AF_INET, addr, dhcp_info, sizeof(dhcp_info));

    LOG_INF("DHCP IP address: %s", dhcp_info);
}
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
#define WIFI_SSID "huihui"
#define WIFI_PSK "61121288b9"

static struct wifi_connect_req_params wifi_params;
static const char wifi_ssid[] = WIFI_SSID; /* Use consistent SSID from header */
static const char wifi_psk[] = WIFI_PSK;   /* Use consistent PSK from header */
static int wifi_connect(void)
{
    LOG_INF("wifi_connect...................");
    struct net_if *iface = net_if_get_first_wifi();

    context.connected = false;
    context.connect_result = false;
    /* Initialize WiFi connection parameters completely */
    memset(&wifi_params, 0, sizeof(wifi_params));

    wifi_params.ssid = (uint8_t *)wifi_ssid;
    wifi_params.ssid_length = strlen(wifi_ssid);
    wifi_params.psk = (uint8_t *)wifi_psk;
    wifi_params.psk_length = strlen(wifi_psk);
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

int bytes_from_str(const char *str, uint8_t *bytes, size_t bytes_len)
{
    size_t i;
    char byte_str[3];

    if (strlen(str) != bytes_len * 2)
    {
        LOG_ERR("Invalid string length: %zu (expected: %d)\n",
                strlen(str), bytes_len * 2);
        return -EINVAL;
    }

    for (i = 0; i < bytes_len; i++)
    {
        memcpy(byte_str, str + i * 2, 2);
        byte_str[2] = '\0';
        bytes[i] = strtol(byte_str, NULL, 16);
    }

    return 0;
}

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


void start_wifi_thread(void)
{
    start_app();
}

void wifi_ready_cb(bool wifi_ready)
{
    LOG_DBG("Is Wi-Fi ready?: %s", wifi_ready ? "yes" : "no");
    wifi_ready_status = wifi_ready;
    k_sem_give(&wifi_ready_state_changed_sem);
}
#endif /* CONFIG_WIFI_READY_LIB */

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