/**
 * \file
 *       ESP8266 RESTful Bridge
 * \author
 *       Tuan PM <tuanpm@live.com>
 */
#ifndef _REST_H_
#define _REST_H_

#include <stdint.h>
#include <stdbool.h>
#include <espduino/espduino.h>

#define DEFAULT_REST_TIMEOUT  5000

typedef enum {
    HEADER_GENERIC = 0,
    HEADER_CONTENT_TYPE,
    HEADER_USER_AGENT
} HEADER_TYPE;

typedef enum {
    HTTP_STATUS_OK = 200
} HTTP_STATUS;

struct esp_rest {
    uint32_t remote_instance;
    uint32_t timeout;
    struct PACKET_CMD *res;
};

void esp_rest_init(struct esp_rest *er);
bool esp_rest_begin(struct esp_rest *, const char *host, uint16_t port,
  bool security);
void esp_rest_request(struct esp_rest *, const char *path, const char *method,
  const char *data, int len);
void esp_rest_get(struct esp_rest *, const char *path, const char *data);
void esp_rest_post(struct esp_rest *, const char *path, const char *data);
void esp_rest_put(struct esp_rest *, const char *path, const char *data);
void esp_rest_del(struct esp_rest *, const char *path, const char *data);

void esp_rest_set_timeout(struct esp_rest *, uint32_t ms);
uint16_t esp_rest_get_response(struct esp_rest *, char *data, uint16_t *lenp);
bool esp_rest_set_user_agent(struct esp_rest *, const char *value);
bool esp_rest_set_content_type(struct esp_rest *, const char *value);
bool esp_rest_set_header(struct esp_rest *, const char *value);

#endif
