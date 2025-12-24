/*
 * WiFi Credentials Storage for Mira Station
 */

#include <zephyr/kernel.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/logging/log.h>

#include "wifi_station.h"  /* Include header for WIFI_SSID/WIFI_PSK constants */

LOG_MODULE_REGISTER(wifi_cred, CONFIG_LOG_DEFAULT_LEVEL);

/* WiFi credentials - UPDATE TO MATCH TARGET NETWORK */
static const char wifi_ssid[] = WIFI_SSID; /* Use consistent SSID from header */
static const char wifi_psk[] = WIFI_PSK; /* Use consistent PSK from header */

/* Static WiFi connection parameters - avoid stack allocation issues */
static struct wifi_connect_req_params wifi_params;

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
	bool is_open_network = (strlen(wifi_psk) == 0); /* "Mimas2" is confirmed as OPEN network */
	
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
