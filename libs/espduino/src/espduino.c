/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <string.h>

#include <os/os.h>
#include <hal/hal_uart.h>

#include <console/console.h>

#include "espduino/espduino.h"
#include "ringbuf.h"
#include "crc16.h"

struct espduino {
    uint8_t e_uart;
    uint8_t e_is_return;
    uint16_t e_cur_cmd;
    esp_req_cb e_cur_cb;
    uint32_t e_return_value;
    RINGBUF e_rx;
    uint8_t e_rx_buf[128];
    RINGBUF e_tx;
    uint8_t e_tx_buf[32];
    struct PROTO e_proto;
    uint8_t e_protobuf[512];
};

static struct espduino esp;

static int espduino_uart_tx(void *arg);
static int espduino_uart_rx(void *arg, uint8_t byte);

int
espduino_init(int uart, uint32_t speed)
{
    struct espduino *e = &esp;
    int rc;

    memset(e, 0, sizeof(*e));
    e->e_uart = uart;
    e->e_proto.buf = e->e_protobuf;
    e->e_proto.bufSize = sizeof(e->e_protobuf);
    e->e_proto.dataLen = 0;
    e->e_proto.isEsc = 0;

    RINGBUF_Init(&e->e_rx, e->e_rx_buf, sizeof(e->e_rx_buf));
    RINGBUF_Init(&e->e_tx, e->e_tx_buf, sizeof(e->e_tx_buf));

    rc = hal_uart_init_cbs(uart, espduino_uart_tx, NULL, espduino_uart_rx,
      &esp);
    if (rc) {
        return -1;
    }
    rc = hal_uart_config(uart, speed, 8, 1, HAL_UART_PARITY_NONE,
      HAL_UART_FLOW_CTL_NONE);
    if (rc) {
        return -1;
    }
    return 0;
}

static int
espduino_uart_tx(void *arg)
{
    struct espduino *e = (struct espduino *)arg;
    uint8_t byte;

    if (RINGBUF_Get(&e->e_tx, &byte)) {
        return -1;
    }
    return byte;
}

static int
espduino_uart_rx(void *arg, uint8_t byte)
{
    struct espduino *e = (struct espduino *)arg;

    if (RINGBUF_Put(&e->e_rx, byte)) {
        return -1;
    }
    return 0;
}

static void
esp_uart_write(struct espduino *e, uint8_t data)
{
    int sr;

    OS_ENTER_CRITICAL(sr);
    while (RINGBUF_Put(&e->e_tx, data) != 0) {
        OS_EXIT_CRITICAL(sr);
        if (os_started()) {
            os_time_delay(1);
        }
        OS_ENTER_CRITICAL(sr);
    }
    OS_EXIT_CRITICAL(sr);
    hal_uart_start_tx(e->e_uart);
}

static int
esp_uart_available(struct espduino *e)
{
    if (e->e_rx.fill_cnt) {
        return 1;
    }
    return 0;
}

static uint8_t
esp_uart_read(struct espduino *e)
{
    int sr;
    uint8_t data;

    OS_ENTER_CRITICAL(sr);
    if (RINGBUF_Get(&e->e_rx, &data)) {
        data = 0;
    }
    OS_EXIT_CRITICAL(sr);
    return data;
}

static void
esp_write_byte(struct espduino *e, uint8_t data)
{
    switch (data) {
    case SLIP_START:
    case SLIP_END:
    case SLIP_REPL:
        esp_uart_write(e, SLIP_REPL);
        esp_uart_write(e, SLIP_ESC(data));
        break;
    default:
        esp_uart_write(e, data);
    }
}

static uint16_t
esp_write_crc(struct espduino *e, const void *data_v, uint16_t len,
  uint16_t crc)
{
    const uint8_t *data = data_v;

    crc = crc16_data(data, len, crc);

    while (len--) {
        esp_write_byte(e, *data);
        data++;
    }
    return crc;
}

uint16_t
esp_request_start(uint16_t cmd, esp_req_cb callback, uint32_t _return,
  uint16_t argc)
{
    struct espduino *e = &esp;
    uint16_t crc = 0;

    esp_uart_write(e, 0x7e);
    crc = esp_write_crc(e, &cmd, 2, crc);
    crc = esp_write_crc(e, &callback, 4, crc);
    crc = esp_write_crc(e, &_return, 4, crc);
    crc = esp_write_crc(e, &argc, 2, crc);

    e->e_cur_cmd = cmd;
    e->e_cur_cb = callback;
    return crc;
}

uint16_t
esp_request_cont(uint16_t crc, const void *data_v, uint16_t len)
{
    struct espduino *e = &esp;
    uint32_t temp = 0;
    uint16_t pad_len = len;

    pad_len = (len + 3) & ~0x3;

    crc = esp_write_crc(e, &pad_len, 2, crc);
    crc = esp_write_crc(e, data_v, len, crc);
    pad_len -= len;
    crc = esp_write_crc(e, &temp, pad_len, crc);

    return crc;
}

void
esp_request_end(uint16_t crc)
{
    struct espduino *e = &esp;
    uint8_t *crcp = (uint8_t *)&crc;

    esp_write_byte(e, *crcp);
    crcp++;
    esp_write_byte(e, *crcp);
    esp_uart_write(e, 0x7F);
}

void
esp_reset(void)
{
    uint16_t crc;

    crc = esp_request_start(CMD_RESET, 0, 0, 0);
    esp_request_end(crc);
}

bool
esp_ready(void)
{
    struct espduino *e = &esp;
    int32_t end;
    uint8_t wait_time;
    uint16_t crc;

    for (wait_time = 5; wait_time > 0; wait_time--) {
        e->e_is_return = 0;
        crc = esp_request_start(CMD_IS_READY, NULL, 1, 0);
        esp_request_end(crc);
        end = os_time_get() + OS_TICKS_PER_SEC;
        while (e->e_is_return == 0 && (end - os_time_get() > 0)) {
            esp_process();
        }
        if (e->e_is_return != 0 && e->e_return_value) {
            return true;
        }
    }
    return false;
}

bool
esp_wait_return(uint32_t *ret_value)
{
    return esp_wait_return_timo(ESP_TIMEOUT, ret_value);
}

void
esp_wait_for(uint16_t cmd, esp_req_cb cb)
{
    struct espduino *e = &esp;

    e->e_cur_cmd = cmd;
    e->e_cur_cb = cb;
}

bool
esp_wait_return_timo(int32_t timeout, uint32_t *ret_value)
{
    struct espduino *e = &esp;
    int32_t end;

    end = os_time_get() + timeout * OS_TICKS_PER_SEC / 1000;

    e->e_is_return = 0;
    while (e->e_is_return == 0 && (end - (int32_t)os_time_get() > 0)) {
        esp_process();
    }
    if (e->e_is_return) {
        if (ret_value) {
            *ret_value = e->e_return_value;
        }
        return true;
    } else {
        return false;
    }
}

void
esp_proto_completed_cb(struct espduino *e)
{
    PACKET_CMD *cmd = (PACKET_CMD *)e->e_proto.buf;
    uint16_t crc = 0;
    uint16_t argc, len, resp_crc;
    uint8_t *data_ptr;

    argc = cmd->argc;

    data_ptr = (uint8_t *)&cmd->args;
    crc = crc16_data(&cmd->cmd, 12, crc);

    while (argc--) {
        len = *((uint16_t *)data_ptr);
        crc = crc16_data(data_ptr, 2, crc);
        data_ptr += 2;
        crc = crc16_data(data_ptr, len, crc);
        data_ptr += len;
    }
    resp_crc = *(uint16_t *)data_ptr;
    if (crc != resp_crc) {
        console_printf("esp: invalid CRC\n");
        return;
    }

    if (e->e_cur_cmd != cmd->cmd) {
        console_printf("esp: got command %d waiting for %d\n",
          e->e_cur_cmd, cmd->cmd);
        return;
    }

    e->e_is_return = 1;
    e->e_return_value = cmd->_return;

    if (e->e_cur_cb != NULL) {
        e->e_cur_cb(cmd);
    }
}

void
esp_process(void)
{
    struct espduino *e = &esp;
    struct PROTO *pr = &e->e_proto;
    char value;

    while (esp_uart_available(e)) {
        value = esp_uart_read(e);
        switch (value) {
        case 0x7D:
            pr->isEsc = 1;
            break;
        case 0x7E:
            pr->dataLen = 0;
            pr->isEsc = 0;
            pr->isBegin = 1;
            break;
        case 0x7F:
            esp_proto_completed_cb(e);
            pr->isBegin = 0;
            break;
        default:
            if (pr->isEsc) {
                value ^= 0x20;
                pr->isEsc = 0;
            }
            if (pr->dataLen < pr->bufSize) {
                pr->buf[pr->dataLen++] = value;
            }
            break;
        }
    }
}
