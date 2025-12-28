#include <zephyr/sys/printk.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <stdlib.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/net/http/client.h>

#if defined(CONFIG_POSIX_API)
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/posix/netdb.h>
#include <zephyr/posix/unistd.h>
#include <zephyr/posix/sys/socket.h>
#endif
#include "common.h"
#include "ca_cert.h"

#define TLS_SEC_TAG 42

extern sd_data_t sd_data;
#define CA_CERT_TAG 1
static const char cert[] = {
#include "DigiCertGlobalG3.pem.inc"

	/* Null terminate certificate if running Mbed TLS on the application core.
	 * Required by TLS credentials API.
	 */
	IF_ENABLED(CONFIG_TLS_CREDENTIALS, (0x00))};

void get_access_token()
{
}

int cert_provision(void)
{
	return tls_credential_add(
		CA_CERT_TAG,
		TLS_CREDENTIAL_CA_CERTIFICATE,
		ca_cert_isrg_root_x1,
		sizeof(ca_cert_isrg_root_x1));
}

int https_post_json(void)
{
	printk("### https_post_json ###\n");
	cert_provision();

	int sock = -1;
	int ret;

	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	struct addrinfo *res = NULL;

	/* HTTPSは443番 */
	ret = getaddrinfo("postman-echo.com", "443", &hints, &res);
	if (ret || res == NULL)
	{
		printk("getaddrinfo failed: %d\n", ret);
		return ret ? ret : -EINVAL;
	}

	/* TCPソケット */
	sock = socket(res->ai_family, SOCK_STREAM, IPPROTO_TLS_1_2);
	if (sock < 0)
	{
		printk("socket failed: %d\n", -errno);
		freeaddrinfo(res);
		return -errno;
	}
	/* ★CA証明書タグをソケットに設定（必須） */
	sec_tag_t sec_tag_list[] = {CA_CERT_TAG};
	ret = setsockopt(sock, SOL_TLS, TLS_SEC_TAG_LIST,
					 sec_tag_list, sizeof(sec_tag_list));
	if (ret < 0)
	{
		printk("TLS_SEC_TAG_LIST failed: %d\n", -errno);
		if (res)
			freeaddrinfo(res);
		if (sock >= 0)
			close(sock);
		printk("### done ret=%d ###\n", ret);
		return ret;
	}
	/* ★SNI/証明書検証用 hostname（必須級） */
	ret = setsockopt(sock, SOL_TLS, TLS_HOSTNAME, sd_data.mcm_iot_hostname, strlen(sd_data.mcm_iot_hostname) + 1);
	if (ret < 0)
	{
		printk("TLS_HOSTNAME failed: %d\n", -errno);
		if (res)
			freeaddrinfo(res);
		if (sock >= 0)
			close(sock);
		printk("### done ret=%d ###\n", ret);
		return ret;
	}

	/* connect */
	ret = connect(sock, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);
	res = NULL;

	if (ret < 0)
	{
		printk("connect failed: %d\n", -errno);
		close(sock);
		return -errno;
	}

	const char json[] = "{\"device\":\"nrf5340\",\"value\":123}";

	char req[512];
	int req_len = snprintf(req, sizeof(req),
						   "POST %s HTTP/1.1\r\n"
						   "Host: %s\r\n"
						   "User-Agent: zephyr\r\n"
						   "Content-Type: application/json\r\n"
						   "Content-Length: %d\r\n"
						   "Connection: close\r\n"
						   "\r\n"
						   "%s",
						   sd_data.endpoint_url[0],
						   sd_data.mcm_iot_hostname,
						   (int)strlen(json), json);

	if (req_len <= 0 || req_len >= (int)sizeof(req))
	{
		printk("snprintf overflow\n");
		close(sock);
		return -ENOMEM;
	}

	printk("### send ###\n");
	ssize_t sent = send(sock, req, req_len, 0);
	if (sent < 0)
	{
		int e = errno;
		printk("send() failed: %d\n", -e);
		close(sock);
		return -e;
	}
	printk("sent=%d\n", (int)sent);

	printk("### recv ###\n");
	char buf[1024];
	int len;

	while ((len = recv(sock, buf, sizeof(buf) - 1, 0)) > 0)
	{
		buf[len] = '\0';
		printk("%s", buf);
	}

	if (len < 0)
	{
		printk("recv() failed: %d\n", -errno);
	}

	close(sock);
	printk("### done ###\n");
	return 0;
}

int http_post_json(void)
{
	printk("### http_post_json ###\n");

	int sock = -1;
	int ret;

	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	struct addrinfo *res = NULL;

	/* HTTPは80番 */
	ret = getaddrinfo("postman-echo.com", "80", &hints, &res);
	if (ret || res == NULL)
	{
		printk("getaddrinfo failed: %d\n", ret);
		return ret ? ret : -EINVAL;
	}

	/* TCPソケット */
	sock = socket(res->ai_family, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
	{
		printk("socket failed: %d\n", -errno);
		freeaddrinfo(res);
		return -errno;
	}

	/* connect */
	ret = connect(sock, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);
	res = NULL;

	if (ret < 0)
	{
		printk("connect failed: %d\n", -errno);
		close(sock);
		return -errno;
	}

	const char json[] = "{\"device\":\"nrf5340\",\"value\":123}";

	char req[512];
	int req_len = snprintf(req, sizeof(req),
						   "POST /post HTTP/1.1\r\n"
						   "Host: postman-echo.com\r\n"
						   "User-Agent: zephyr\r\n"
						   "Content-Type: application/json\r\n"
						   "Content-Length: %d\r\n"
						   "Connection: close\r\n"
						   "\r\n"
						   "%s",
						   (int)strlen(json), json);

	if (req_len <= 0 || req_len >= (int)sizeof(req))
	{
		printk("snprintf overflow\n");
		close(sock);
		return -ENOMEM;
	}

	printk("### send ###\n");
	ssize_t sent = send(sock, req, req_len, 0);
	if (sent < 0)
	{
		int e = errno;
		printk("send() failed: %d\n", -e);
		close(sock);
		return -e;
	}
	printk("sent=%d\n", (int)sent);

	printk("### recv ###\n");
	char buf[1024];
	int len;

	while ((len = recv(sock, buf, sizeof(buf) - 1, 0)) > 0)
	{
		buf[len] = '\0';
		printk("%s", buf);
	}

	if (len < 0)
	{
		printk("recv() failed: %d\n", -errno);
	}

	close(sock);
	printk("### done ###\n");
	return 0;
}
