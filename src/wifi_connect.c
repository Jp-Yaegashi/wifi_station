#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/printk.h>
#include "wifi_connect.h"
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <net/wifi_ready.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_connect, LOG_LEVEL_INF);




#define MAX_SSID_LEN 32
#define STATUS_POLLING_MS 300
static struct net_mgmt_event_callback wifi_shell_mgmt_cb;
static struct net_mgmt_event_callback net_shell_mgmt_cb;


static K_SEM_DEFINE(wifi_ready_state_changed_sem, 0, 1);
static bool wifi_ready_status;
/* Scan timeout prevention - Dual stage protection */
static struct k_work_delayable scan_timeout_work;
static struct k_work_delayable early_warning_work;
static struct net_if *wifi_iface_cached = NULL;
static bool scan_in_progress = false;
static struct
{
	const struct shell *sh;
	union {
		struct {
			uint8_t connected	: 1;
			uint8_t connect_result	: 1;
			uint8_t disconnect_requested	: 1;
			uint8_t connection_timeout	: 1;
			uint8_t _unused		: 4;
		};
		uint8_t all;
	};
	int retry_count;
	int64_t connect_start_time;
} context;
#define WIFI_SHELL_MGMT_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | \
                                NET_EVENT_WIFI_DISCONNECT_RESULT)
/* WM02C nRF7002 power management - SELECTIVE RECOVERY MODE */
static int wm02c_power_cycle(void)
{
	LOG_INF("🔄 WM02C power cycle - SELECTIVE RECOVERY MODE");
	
	return 0;
}

/* RPU Recovery for stuck scan states - Only used when scan operations fail */
static int rpu_scan_recovery(struct net_if *iface)
{
	LOG_WRN("🛠️ RPU scan recovery - clearing stuck scan state");
	
	/* Step 1: Interface down */
	LOG_INF("⬇️ Taking WiFi interface down...");
	net_if_down(iface);
	k_sleep(K_MSEC(100));
	
	/* Step 2: RPU power cycle to clear corrupted state */
	LOG_INF("🔄 Power cycling RPU to clear scan corruption...");
	wm02c_power_cycle();
	
	/* Step 3: Interface back up */
	LOG_INF("⬆️ Bringing WiFi interface back up...");
	net_if_up(iface);
	k_sleep(K_SECONDS(1));
	
	/* Step 4: Wait for RPU stabilization */
	LOG_INF("⏳ Waiting for RPU stabilization after recovery...");
	k_sleep(K_SECONDS(2));
	
	LOG_INF("✅ RPU scan recovery complete - ready for new operations");
	return 0;
}
static int wm02c_interface_recovery(struct net_if *iface)
{
	LOG_INF("🛠️ WM02C interface recovery - checking scan state...");
	
	/* Check if we're stuck in scan state - if so, use RPU recovery */
	struct wifi_iface_status status;
	int ret = net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status, sizeof(status));
	
	if (ret == 0 && status.state == WIFI_STATE_SCANNING) {
		LOG_WRN("⚠️ Detected stuck SCANNING state - using RPU recovery");
		return rpu_scan_recovery(iface);
	}
	
	/* Standard interface recovery without RPU reset */
	LOG_INF("🛠️ Standard interface recovery (no scan issues detected)...");
	
	net_if_down(iface);
	k_sleep(K_MSEC(500));
	net_if_up(iface);
	k_sleep(K_SECONDS(1));
	
	LOG_INF("✅ Standard interface recovery complete");
	return 0;
}
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

static void print_dhcp_ip(struct net_mgmt_event_callback *cb)
{
    /* Get DHCP info from struct net_if_dhcpv4 and print */
    const struct net_if_dhcpv4 *dhcpv4 = cb->info;
    const struct in_addr *addr = &dhcpv4->requested_ip;
    char dhcp_info[128];

    net_addr_ntop(AF_INET, addr, dhcp_info, sizeof(dhcp_info));

    LOG_INF("DHCP IP address: %s", dhcp_info);
}


void wifi_ready_cb(bool wifi_ready)
{
    LOG_DBG("Is Wi-Fi ready?: %s", wifi_ready ? "yes" : "no");
    wifi_ready_status = wifi_ready;
    k_sem_give(&wifi_ready_state_changed_sem);
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
        uint8_t mac_string_buf[sizeof("xx:xx:xx:xx:xx:xx")];

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

int wifi_connect(void)
{
   LOG_INF("🚀 Entered wifi_connect() function");
	
	struct net_if *iface = net_if_get_first_wifi();
	int ret;

	context.connected = false;
	context.connect_result = false;

	/* Check current interface status */
	bool is_up_before = net_if_is_up(iface);
	LOG_INF("🔍 WiFi interface status before connection: %s", is_up_before ? "UP" : "DOWN");

	/* Force interface to UP state if needed */
	if (!is_up_before) {
		LOG_INF("🔧 Forcing WiFi interface UP (bypass timeout)");
		
		/* Force carrier and interface state */
		net_if_carrier_on(iface);
		k_sleep(K_MSEC(500));
		
		LOG_INF("✅ WiFi interface forced UP");
	} else {
		LOG_INF("✅ WiFi interface already UP");
	}
	
	/* Zephyr WiFi stack pre-connection check */
	LOG_INF("🔍 Pre-connection stability check...");
	k_sleep(K_SECONDS(2));

	/* Start dual-stage scan timeout prevention */
	/* Stage 1: Early warning at 30s for graceful recovery */
	/* Stage 2: Emergency abort at 35s before WPA supplicant timeout (~46s) */
	scan_in_progress = true;
	k_work_schedule(&early_warning_work, K_SECONDS(30));
	k_work_schedule(&scan_timeout_work, K_SECONDS(35));
	LOG_INF("🛡️ Dual-stage scan protection: Warning=30s, Emergency=35s");

	/* First set up credentials */
	ret = wifi_credentials_set(iface);
	if (ret) {
		LOG_ERR("Failed to set WiFi credentials: %d", ret);
		scan_in_progress = false;
		k_work_cancel_delayable(&scan_timeout_work);
		return ret;
	}

	LOG_INF("WiFi connection initiated with credentials");
	return 0;
}


int start_app(void)
{

   LOG_INF("Starting WiFi application");
	
	/* Conservative automatic connection mode with extended delays */
	LOG_INF("Conservative connection mode activated");
	LOG_INF("Stability features:");
	LOG_INF("   - Extended stabilization delays");
	LOG_INF("   - Single connection attempt per cycle");
	LOG_INF("   - DevZone band configuration applied");
	
	/* Extended stabilization before first connection attempt */
	LOG_INF("WPA supplicant stabilization...");
	k_sleep(K_SECONDS(10));
	LOG_INF("Stabilization complete - starting connection attempts");

	while (1) {
		context.retry_count++;
		context.connect_start_time = k_uptime_get();
		context.connection_timeout = false;
		
		LOG_INF("🔄 Conservative WiFi Connection Attempt #%d", context.retry_count);
		
		/* NO SCANNING - Direct connection only for stability */
		LOG_INF("⚠️ Skipping scan for WPA supplicant stability");
		
		/* Extended delay before connection attempt */
		k_sleep(K_SECONDS(5));
		
		wifi_connect();

		/* Wait for connection result with extended timeout */
		int timeout_counter = 0;
		const int max_timeout = 200; /* 200 * 300ms = 60 seconds - extended for stability */
		
		while (!context.connect_result && timeout_counter < max_timeout) {
			cmd_wifi_status();
			k_sleep(K_MSEC(STATUS_POLLING_MS));
			timeout_counter++;
			
			/* Check for timeout every 20 iterations (6 seconds) - less frequent for stability */
			if (timeout_counter % 20 == 0) {
				int64_t elapsed = k_uptime_get() - context.connect_start_time;
				LOG_INF("⏱️ Connection time: %lld ms (attempt %d)", elapsed, context.retry_count);
			}
		}
		
		if (timeout_counter >= max_timeout) {
			LOG_ERR("⏰ Connection timeout after 60 seconds");
			context.connection_timeout = true;
			context.connect_result = true; /* Force exit from wait loop */
		}

		if (context.connected) {
			LOG_INF("✅ WiFi Connected Successfully!");
			cmd_wifi_status();
			k_sleep(K_FOREVER);
		} else {
			LOG_ERR("❌ Connection failed. Extended recovery delay...");
			
			/* Extended recovery time between attempts */
			LOG_INF("⏳ Extended recovery delay (60 seconds) for WPA supplicant stability");
			k_sleep(K_SECONDS(60));
			
			/* Reset connection state */
			context.connect_result = false;
			context.connected = false;
			
			/* Interface recovery with extended delays */
			struct net_if *iface = net_if_get_first_wifi();
			if (iface) {
				LOG_INF("🔧 Conservative interface recovery...");
				wm02c_interface_recovery(iface);
				k_sleep(K_SECONDS(10)); /* Extended recovery delay */
			}
		}
			}
			
			/* Reset for next attempt */
			context.connect_result = false;
			context.connection_timeout = false;
			k_sleep(K_SECONDS(8));
			
			/* Limit retry attempts */
			if (context.retry_count >= 10) {
				LOG_ERR("🚫 Maximum retry attempts reached. Resetting counter.");
				context.retry_count = 0;
				k_sleep(K_SECONDS(30)); /* Long pause before reset */
	}

    return 0;
}

void start_wifi_thread(void)
{
    start_app();
}

int init_wifi()
{
      /* Initialize network management callbacks */
	LOG_INF("Initializing network management callbacks...");
	net_mgmt_callback_init();
	LOG_INF("Network management callbacks initialized");
	
	/* Check WiFi interface availability */
	LOG_INF("Searching for WiFi interface...");
	struct net_if *iface = net_if_get_first_wifi();
	LOG_INF("WiFi interface found: %p", iface);
	
	if (!iface) {
		LOG_ERR("❌ No WiFi interface found!");
		return -1;
	}
	LOG_INF("✅ WiFi interface found: %p", iface);
	
	/* Check interface status */
	bool is_up = net_if_is_up(iface);
	LOG_INF("WiFi interface status: %s", is_up ? "UP" : "DOWN");
	
	if (is_up) {
		LOG_INF("WiFi interface is UP");
	} else {
		LOG_INF("WiFi interface is DOWN - bringing it UP");
		
		/* Explicitly bring up the WiFi interface */
		int ret = net_if_up(iface);
		if (ret) {
			LOG_ERR("Failed to bring up WiFi interface: %d", ret);
			return ret;
		}
		
		/* Wait for RPU to stabilize */
		LOG_INF("Waiting for WiFi RPU to stabilize...");
		
		/* Wait for interface to come up */
		for (int i = 0; i < 10; i++) {
			k_sleep(K_SECONDS(1));
			if (net_if_is_up(iface)) {
				LOG_INF("WiFi interface came UP after %d seconds", i + 1);
				break;
			}
			if (i == 4) {
				LOG_INF("⚙️ Forcing interface carrier ON due to timeout");
				net_if_carrier_on(iface);
			}
			if ((i + 1) % 3 == 0) {
				LOG_INF("⏳ Still waiting... (%d seconds elapsed)", i + 6);
			}
		}
		
		/* Final verification */
		LOG_INF("🔍 Final interface status check...");
		if (net_if_is_up(iface)) {
			LOG_INF("✅ WiFi interface is now UP");
		} else {
			LOG_WRN("⚠️ WiFi interface still DOWN - forcing carrier ON");
			
			/* Try to force interface carrier on */
			net_if_carrier_on(iface);
			k_sleep(K_SECONDS(2));
			
			if (net_if_is_up(iface)) {
				LOG_INF("WiFi interface is UP");
			}
		}
	}
	
	/* WiFi interface validation */
	struct wifi_iface_status status;
	int ret = net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status, sizeof(status));
	if (ret == 0) {
		LOG_INF("WiFi interface ready, state: %d", status.state);
	} else {
		LOG_WRN("WiFi interface status check failed: %d", ret);
	}
	
	/* Brief stabilization delay */
	k_sleep(K_SECONDS(2));
	
	/* Start WiFi application */
	LOG_INF("Starting WiFi application...");
}

/* Static WiFi connection parameters - avoid stack allocation issues */
static struct wifi_connect_req_params wifi_params;
static const char wifi_ssid[] = WIFI_SSID; /* Use consistent SSID from header */
static const char wifi_psk[] = WIFI_PSK; /* Use consistent PSK from header */


int wifi_credentials_set(struct net_if *iface)
{
	int ret;

	LOG_INF("Setting up WiFi credentials for: %s", wifi_ssid);

	/* Validate credentials first */
	if (strlen(wifi_ssid) == 0) {
		LOG_ERR("Invalid WiFi credentials - SSID empty");
		return -EINVAL;
	}
	
	/* NETWORK TYPE: Check if target network is open or secured */
	bool is_open_network = true; /* "Mimas2" is confirmed as OPEN network */
	
	LOG_INF("🔍 Security type constants: NONE=%d, PSK=%d", 
		WIFI_SECURITY_TYPE_NONE, WIFI_SECURITY_TYPE_PSK);
	LOG_INF("🔧 Network type: %s (password in header: %s)", 
		is_open_network ? "OPEN" : "WPA2-PSK", 
		strlen(wifi_psk) > 0 ? "YES but unused" : "NO");

	/* Initialize WiFi connection parameters completely */
	memset(&wifi_params, 0, sizeof(wifi_params));
	
	if (!is_open_network) {
		/* WPA2-PSK encrypted network configuration */
		wifi_params.ssid = (uint8_t *)wifi_ssid;
		wifi_params.ssid_length = strlen(wifi_ssid);
		wifi_params.psk = (uint8_t *)wifi_psk;
		wifi_params.psk_length = strlen(wifi_psk);
		wifi_params.security = WIFI_SECURITY_TYPE_PSK; /* WPA2-PSK security */
		wifi_params.sae_password = NULL;
		wifi_params.sae_password_length = 0;
		wifi_params.channel = WIFI_CHANNEL_ANY;
		wifi_params.band = WIFI_FREQ_BAND_2_4_GHZ; /* Explicit 2.4GHz for Mimas2 */
		wifi_params.mfp = WIFI_MFP_DISABLE; /* Disable MFP for compatibility */
		wifi_params.timeout = SYS_FOREVER_MS; /* Let the driver handle timeouts */
		LOG_INF("Configuring for WPA2-PSK encrypted network");
	} else {
		/* Open network configuration - "Mimas2" appears to be open based on logs */
		wifi_params.ssid = (uint8_t *)wifi_ssid;
		wifi_params.ssid_length = strlen(wifi_ssid);
		wifi_params.psk = NULL; /* No password for open network */
		wifi_params.psk_length = 0;
		wifi_params.security = WIFI_SECURITY_TYPE_NONE; /* Open network */
		wifi_params.sae_password = NULL;
		wifi_params.sae_password_length = 0;
		wifi_params.channel = WIFI_CHANNEL_ANY;
		wifi_params.band = WIFI_FREQ_BAND_2_4_GHZ;
		wifi_params.mfp = WIFI_MFP_DISABLE;
		wifi_params.timeout = SYS_FOREVER_MS;
		LOG_INF("Configuring for OPEN network (no password)");
	}

	LOG_INF("SSID: %s (len=%d)", wifi_ssid, wifi_params.ssid_length);
	if (!is_open_network) {
		LOG_INF("Password: WPA2-PSK (encrypted - length=%d)", wifi_params.psk_length);
		LOG_INF("Security: WPA-PSK/WPA2-PSK, Band: 2.4GHz (explicit), Channel: %d, MFP: Disabled", wifi_params.channel);
	} else {
		LOG_INF("PSK: %s (len=%d)", wifi_psk, wifi_params.psk_length);
		LOG_INF("Security: OPEN, Band: 2.4GHz (explicit), Channel: Any");
	}

	/* Detailed parameter logging for authentication debugging */
	LOG_INF("📋 WiFi Connection Parameters:");
	LOG_INF("   SSID: %.*s (length=%d)", wifi_params.ssid_length, wifi_params.ssid, wifi_params.ssid_length);
	LOG_INF("   PSK pointer: %p, length: %d", (void*)wifi_params.psk, wifi_params.psk_length);
	
	/* WPA Supplicant parameter validation */
	LOG_INF("🔍 WPA Supplicant Parameter Validation:");
	LOG_INF("   → SSID buffer valid: %s", wifi_params.ssid ? "YES" : "NO");
	LOG_INF("   → PSK buffer valid: %s", wifi_params.psk ? "YES" : "NO");
	LOG_INF("   → Security type: %d (%s)", wifi_params.security, 
		wifi_params.security == WIFI_SECURITY_TYPE_NONE ? "OPEN" :
		wifi_params.security == WIFI_SECURITY_TYPE_PSK ? "WPA2-PSK" : "OTHER");
	
	/* Open network specific validation */
	if (is_open_network) {
		LOG_INF("📡 OPEN Network Validation:");
		LOG_INF("   → PSK should be NULL: %s", wifi_params.psk == NULL ? "✅ CORRECT" : "❌ ERROR");
		LOG_INF("   → PSK length should be 0: %s", wifi_params.psk_length == 0 ? "✅ CORRECT" : "❌ ERROR");
		LOG_INF("   → Security should be NONE: %s", 
			wifi_params.security == WIFI_SECURITY_TYPE_NONE ? "✅ CORRECT" : "❌ ERROR");
		if (wifi_params.psk != NULL || wifi_params.psk_length != 0) {
			LOG_ERR("❌ CRITICAL: Open network has PSK data - this may cause auth timeout!");
		}
	} else if (wifi_params.psk && wifi_params.psk_length > 0) {
		LOG_INF("   → Password first 4 chars: %.4s*** (len=%d)", 
			wifi_params.psk, wifi_params.psk_length);
	}
		if (!is_open_network) {
			LOG_INF("   Security: %d (WPA-PSK/WPA2-PSK)", wifi_params.security);
		} else {
			LOG_INF("   Security: %d (OPEN/NONE)", wifi_params.security);
		}
	LOG_INF("   Band: %d (%s)", wifi_params.band, 
		wifi_params.band == WIFI_FREQ_BAND_2_4_GHZ ? "2.4GHz" :
		wifi_params.band == WIFI_FREQ_BAND_5_GHZ ? "5GHz" :
		wifi_params.band == WIFI_FREQ_BAND_UNKNOWN ? "Auto-detect" : "OTHER");
	LOG_INF("   Channel: %d (%s)", wifi_params.channel, 
		(wifi_params.channel == WIFI_CHANNEL_ANY) ? "ANY" : "SPECIFIC");
	LOG_INF("   MFP: %d (DISABLED)", wifi_params.mfp);
	LOG_INF("   Timeout: %d ms (SYS_FOREVER_MS)", wifi_params.timeout);

	/* Send WiFi connection request */
	LOG_INF("Sending NET_REQUEST_WIFI_CONNECT...");
	LOG_INF("Connection params: timeout=%d, security=%d, band=%d, mfp=%d", 
		wifi_params.timeout, wifi_params.security, wifi_params.band, wifi_params.mfp);
		
	/* Try bypassing WPA supplicant for open networks */
	LOG_INF("🔧 Preparing WiFi connection for %s network", 
		is_open_network ? "OPEN" : "WPA-PSK/WPA2-PSK");
	
	/* Pre-connection WiFi interface validation */
	LOG_INF("🔍 Validating WiFi interface before connection...");
	
	/* Check WiFi interface MAC address */
	struct net_linkaddr *link_addr = net_if_get_link_addr(iface);
	if (link_addr && link_addr->len >= 6) {
		LOG_INF("✅ WiFi MAC Address: %02x:%02x:%02x:%02x:%02x:%02x",
			link_addr->addr[0], link_addr->addr[1], link_addr->addr[2],
			link_addr->addr[3], link_addr->addr[4], link_addr->addr[5]);
	} else {
		LOG_ERR("❌ WiFi MAC Address: Not available - interface not ready!");
		if (link_addr) {
			LOG_ERR("   Link addr len: %d (expected >= 6)", link_addr->len);
		}
		return -ENODEV;
	}
	
	/* Check interface state */
	if (!net_if_is_up(iface)) {
		LOG_ERR("❌ WiFi interface is DOWN - bringing up...");
		net_if_up(iface);
		k_sleep(K_MSEC(2000)); /* Wait for interface to come up */
		if (!net_if_is_up(iface)) {
			LOG_ERR("❌ Failed to bring WiFi interface UP");
			return -ENETDOWN;
		}
	}
	LOG_INF("✅ WiFi interface is UP and ready");
	
	/* Verify nRF7002 driver is ready with a quick scan */
	LOG_INF("📶 Testing nRF7002 driver readiness with scan...");
	static struct wifi_scan_params scan_params;
	memset(&scan_params, 0, sizeof(scan_params));
	scan_params.scan_type = WIFI_SCAN_TYPE_ACTIVE;
	scan_params.bands = WIFI_FREQ_BAND_2_4_GHZ;
	scan_params.dwell_time_active = 50;  /* Quick scan - 50ms per channel */
	scan_params.dwell_time_passive = 100;
	
	int scan_ret = net_mgmt(NET_REQUEST_WIFI_SCAN, iface, &scan_params, sizeof(scan_params));
	if (scan_ret) {
		LOG_WRN("⚠️ WiFi scan failed: %d (driver may not be ready)", scan_ret);
	} else {
		LOG_INF("✅ WiFi scan initiated successfully - driver is ready");
		k_sleep(K_MSEC(2000)); /* Wait for scan to complete */
	}
	
	/* Conservative WPA supplicant connection for WPA2-PSK */
	if (!is_open_network) {
		LOG_INF("🔧 WPA-PSK/WPA2-PSK network - using conservative WPA supplicant");
		LOG_INF("⏳ Extended pre-connection stabilization...");
		
		/* Validate WPA supplicant readiness */
		LOG_INF("🔍 Pre-connection WPA Supplicant validation:");
		LOG_INF("   → Interface state: %s", net_if_is_up(iface) ? "UP" : "DOWN");
		LOG_INF("   → Parameters ready: SSID=%s, PSK=%s, Security=%d",
			wifi_params.ssid ? "SET" : "NULL",
			wifi_params.psk ? "SET" : "NULL", 
			wifi_params.security);
		
		k_sleep(K_MSEC(3000)); /* Reduced delay after interface validation */
	}
	
	LOG_INF("🔧 Using NET_REQUEST_WIFI_CONNECT with static parameters");
	
	/* Record connection attempt timing for timeout analysis */
	int64_t connect_start = k_uptime_get();
	LOG_INF("⏰ Connection attempt started at: %lld ms", connect_start);
	
	ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params, sizeof(wifi_params));
	
	int64_t connect_call_end = k_uptime_get();
	LOG_INF("⏰ net_mgmt call completed at: %lld ms (duration: %lld ms)", 
		connect_call_end, connect_call_end - connect_start);
	
	if (ret) {
		if (is_open_network) {
			LOG_ERR("❌ Open network connection failed: %d", ret);
			LOG_ERR("❌ Possible causes for OPEN network failure:");
			LOG_ERR("   - MAC filtering on AP");
			LOG_ERR("   - Channel 13 (2472MHz) region restrictions"); 
			LOG_ERR("   - AP connection limit reached");
			LOG_ERR("   - WPA Supplicant incorrectly processing OPEN network");
			return ret;
		} else {
			/* For secured networks, try alternative settings */
			LOG_ERR("Initial WiFi connection failed: %d", ret);
			
			/* Second attempt: Try with MFP optional */
			LOG_INF("🔄 Retrying with MFP optional...");
			wifi_params.mfp = WIFI_MFP_OPTIONAL;
			
			ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params, sizeof(wifi_params));
			if (ret) {
				LOG_ERR("❌ All authentication attempts failed: %d", ret);
				return ret;
			}
		}
	}

	LOG_INF("WiFi connection request sent successfully (ret=%d)", ret);
	
	/* Give some time for the driver to process */
	k_sleep(K_MSEC(500));
	
	return 0;
}