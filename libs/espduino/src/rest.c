/**
 * \file
 *       ESP8266 RESTful Bridge
 * \author
 *       Tuan PM <tuanpm@live.com>
 */

#include <string.h>
#include "espduino/espduino.h"
#include "espduino/rest.h"

static struct esp_rest *esp_rest_in_prog;

void
esp_rest_init(struct esp_rest *er)
{
    memset(er, 0, sizeof(*er));
    er->remote_instance = 0;
    er->timeout = DEFAULT_REST_TIMEOUT;
}

bool
esp_rest_begin(struct esp_rest *er, const char *host, uint16_t port,
  bool security)
{
    uint8_t sec = 0;
    uint16_t crc;

    if (security) {
        sec = 1;
    }

    crc = esp_request_start(CMD_REST_SETUP, NULL, 1, 3);
    crc = esp_request_cont(crc, host, strlen(host));
    crc = esp_request_cont(crc, &port, 2);
    crc = esp_request_cont(crc, &sec, 1);
    esp_request_end(crc);

    if (esp_wait_return_timo(er->timeout, &er->remote_instance) &&
      er->remote_instance != 0) {
        return true;
    }
    return false;
}

void
esp_rest_request(struct esp_rest *er, const char *path, const char *method,
  const char *data, int len)
{
    uint16_t crc;

    if (er->remote_instance == 0) {
        return;
    }
    if(len > 0) {
        crc = esp_request_start(CMD_REST_REQUEST, 0, 0, 5);
    } else {
        crc = esp_request_start(CMD_REST_REQUEST, 0, 0, 3);
    }
    crc = esp_request_cont(crc, &er->remote_instance, 4);
    crc = esp_request_cont(crc, method, strlen(method));
    crc = esp_request_cont(crc, path, strlen(path));
    if (len > 0) {
        crc = esp_request_cont(crc, &len, 2);
        crc = esp_request_cont(crc, data, len);
    }
    esp_request_end(crc);
}

void
esp_rest_get(struct esp_rest *er, const char *path, const char *data)
{
    esp_rest_request(er, path, "GET", data, strlen(data));
}

void
esp_rest_post(struct esp_rest *er, const char *path, const char *data)
{
    esp_rest_request(er, path, "POST", data, strlen(data));
}

void
esp_rest_put(struct esp_rest *er, const char *path, const char *data)
{
    esp_rest_request(er, path, "PUT", data, strlen(data));
}

void
esp_rest_del(struct esp_rest *er, const char *path, const char *data)
{
    esp_rest_request(er, path, "DELETE", data, strlen(data));
}

bool
esp_rest_set_header(struct esp_rest *er, const char *value)
{
    uint8_t header_index = HEADER_GENERIC;
    uint16_t crc;

    crc = esp_request_start(CMD_REST_SETHEADER, NULL, 0, 3);
    crc = esp_request_cont(crc, &er->remote_instance, 4);
    crc = esp_request_cont(crc, &header_index, 1);
    crc = esp_request_cont(crc, value, strlen(value));
    esp_request_end(crc);
    return esp_wait_return_timo(er->timeout, NULL);
}

bool
esp_rest_set_content_type(struct esp_rest *er, const char *value)
{
    uint8_t header_index = HEADER_CONTENT_TYPE;
    uint16_t crc;

    crc = esp_request_start(CMD_REST_SETHEADER, NULL, 0, 3);
    crc = esp_request_cont(crc, &er->remote_instance, 4);
    crc = esp_request_cont(crc, &header_index, 1);
    crc = esp_request_cont(crc, value, strlen(value));
    esp_request_end(crc);
    return esp_wait_return_timo(er->timeout, NULL);
}

bool
esp_rest_set_user_agent(struct esp_rest *er, const char *value)
{
    uint8_t header_index = HEADER_USER_AGENT;
    uint16_t crc;

    crc = esp_request_start(CMD_REST_SETHEADER, 0, 0, 3);
    crc = esp_request_cont(crc, &er->remote_instance, 4);
    crc = esp_request_cont(crc, &header_index, 1);
    crc = esp_request_cont(crc, value, strlen(value));
    esp_request_end(crc);
    return esp_wait_return_timo(er->timeout, NULL);
}

void
esp_rest_set_timeout(struct esp_rest *er, uint32_t ms)
{
    er->timeout = ms;
}

static void
esp_rest_cb(struct PACKET_CMD *cmd)
{
    if (esp_rest_in_prog) {
        esp_rest_in_prog->res = cmd;
    }
}

uint16_t
esp_rest_get_response(struct esp_rest *er, char *data, uint16_t *lenp)
{
    bool rc;
    uint16_t len;
    uint32_t http_rc;

    er->res = NULL;
    esp_rest_in_prog = er;
    esp_wait_for(CMD_REST_EVENTS, esp_rest_cb);
    rc = esp_wait_return_timo(er->timeout, &http_rc);
    esp_rest_in_prog = NULL;
    if (rc != true || er->res == NULL) {
        return 0;
    }
    len = *lenp;
    if (len > er->res->args.len) {
        len = er->res->args.len;
    }
    memcpy(data, &er->res->args.data, len);
    *lenp = len;

    return http_rc;
}
