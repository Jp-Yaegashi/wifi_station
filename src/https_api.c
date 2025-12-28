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

#define TLS_SEC_TAG 42

extern sd_data_t sd_data;

static const char cert[] = {
#include "DigiCertGlobalG3.pem.inc"

	/* Null terminate certificate if running Mbed TLS on the application core.
	 * Required by TLS credentials API.
	 */
	IF_ENABLED(CONFIG_TLS_CREDENTIALS, (0x00))};

void get_access_token()
{
}
void send_post_request(int sock)
{
	/*
	struct http_request req = {0};
	// 送信したい JSON データ
	const char json_payload[] = "{\"key\": \"value\", \"sensor\": 123}";

	// 1. 基本設定
	req.method = HTTP_POST;          // POSTメソッドを指定
	req.url = sd_data.endpoint_url[0];       // エンドポイント
	req.host = sd_data.mcm_iot_hostname;
	req.protocol = "HTTP/1.1";

	// 2. ヘッダーの設定 (重要: JSONであることを伝える)
	const char *headers[] = {
		"Content-Type: application/json\r\n",
		NULL
	};
	req.header_fields = headers;

	// 3. JSONデータのセット
	req.payload = json_payload;
	req.payload_len = strlen(json_payload);

	// 4. 送信実行
	// http_client_req() を呼び出す (タイムアウト設定など)
	int err = http_client_req(sock, &req, 5000, NULL);
	if (err < 0) {
		printk("Failed to send HTTP POST: %d\n", err);
	}*/
}

int https_post_json(void)
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
    if (ret || res == NULL) {
        printk("getaddrinfo failed: %d\n", ret);
        return ret ? ret : -EINVAL;
    }

    /* TCPソケット */
    sock = socket(res->ai_family, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        printk("socket failed: %d\n", -errno);
        freeaddrinfo(res);
        return -errno;
    }

    /* connect */
    ret = connect(sock, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    res = NULL;

    if (ret < 0) {
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

    if (req_len <= 0 || req_len >= (int)sizeof(req)) {
        printk("snprintf overflow\n");
        close(sock);
        return -ENOMEM;
    }

    printk("### send ###\n");
    ssize_t sent = send(sock, req, req_len, 0);
    if (sent < 0) {
        int e = errno;
        printk("send() failed: %d\n", -e);
        close(sock);
        return -e;
    }
    printk("sent=%d\n", (int)sent);

    printk("### recv ###\n");
    char buf[1024];
    int len;

    while ((len = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[len] = '\0';
        printk("%s", buf);
    }

    if (len < 0) {
        printk("recv() failed: %d\n", -errno);
    }

    close(sock);
    printk("### done ###\n");
    return 0;
}
