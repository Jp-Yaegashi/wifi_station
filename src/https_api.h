/*
 * WiFi  Header for Mira Station
 */

#ifndef HTTPS_API_H
#define HTTPS_API_H


void send_post_request(int sock);
int https_post_json(void);
int http_post_json(void);
#endif