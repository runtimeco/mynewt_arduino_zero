/**
 * \file
 *       ESP8266 bridge arduino library
 * \author
 *       Tuan PM <tuanpm@live.com>
 */
#ifndef _ARDUINO_WIFI_H_
#define _ARDUINO_WIFI_H_

/*
 * Note that this library only works if the byte-order of ESP is the same
 * as the MCU this package runs on.
 */
#include <stdint.h>
#include <stdbool.h>

#define ESP_TIMEOUT 2000

#define SLIP_START 0x7E
#define SLIP_END  0x7F
#define SLIP_REPL 0x7D
#define SLIP_ESC(x) (x ^ 0x20)

/*
 * Command codes for thing sent to ESPLink.
 */
typedef enum CMD_NAME {
    CMD_NULL = 0,
    CMD_RESET,
    CMD_IS_READY,
    CMD_WIFI_CONNECT,
    CMD_MQTT_SETUP,
    CMD_MQTT_CONNECT,
    CMD_MQTT_DISCONNECT,
    CMD_MQTT_PUBLISH,
    CMD_MQTT_SUBSCRIBE,
    CMD_MQTT_LWT,
    CMD_MQTT_EVENTS,
    CMD_REST_SETUP,
    CMD_REST_REQUEST,
    CMD_REST_SETHEADER,
    CMD_REST_EVENTS
} CMD_NAME;

enum WIFI_STATUS {
    STATION_IDLE = 0,
    STATION_CONNECTING,
    STATION_WRONG_PASSWORD,
    STATION_NO_AP_FOUND,
    STATION_CONNECT_FAIL,
    STATION_GOT_IP
};

typedef struct PROTO {
    uint8_t *buf;
    uint16_t bufSize;
    uint16_t dataLen;
    uint8_t isEsc;
    uint8_t isBegin;
} PROTO;

struct ARGS {
    uint16_t len;
    uint8_t data;
} __attribute((__packed__));

typedef struct ARGS ARGS;

/*
 * Header in frames exchanged with ESP-link. For asyncronous notifications,
 * like wifi status change, 'callback' used initially gets passed back.
 */
struct PACKET_CMD {
    uint16_t cmd;
    uint32_t callback;
    uint32_t _return;
    uint16_t argc;
    ARGS args;
} __attribute((__packed__));

typedef struct PACKET_CMD PACKET_CMD;

typedef void (*esp_req_cb)(struct PACKET_CMD *);

int espduino_init(int uart, uint32_t speed);
bool esp_ready(void);
void esp_reset(void);

/*
 * Transmit request, data is CRC'd while sending.
 */
uint16_t esp_request_start(uint16_t cmd, esp_req_cb callback, uint32_t _return,
  uint16_t argc);
uint16_t esp_request_cont(uint16_t crc, const void *data_v, uint16_t len);
void esp_request_end(uint16_t crc);

/*
 * Data from ESPLink is read within esp_process().
 * esp_wait_return() and esp_wait_return_timo() call esp_process() internally.
 */
void esp_process(void);
bool esp_wait_return(uint32_t *ret_value);
bool esp_wait_return_timo(int32_t timeout, uint32_t *ret_value);

/*
 * Normally library expects response to have command code matching the
 * request. But not always, esp_wait_for() changes what response is
 * expected.
 */
void esp_wait_for(uint16_t cmd, esp_req_cb callback);

#endif
