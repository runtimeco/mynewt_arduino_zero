/*
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
#include <os/os.h>
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash.h>
#include <console/console.h>
#include <hal/hal_uart.h>
#include <hal/hal_system.h>
#include <hal/hal_cputime.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <os/os_time.h>
#include <id/id.h>

#include <shell/shell.h>

#include <espduino/espduino.h>
#include <espduino/rest.h>
#include <ctype.h>

#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif

/* Init all tasks */
volatile int tasks_initialized;
int init_tasks(void);

#define DEFAULT_MBUF_MPOOL_BUF_LEN (256)
#define DEFAULT_MBUF_MPOOL_NBUFS (10)

uint8_t default_mbuf_mpool_data[DEFAULT_MBUF_MPOOL_BUF_LEN *
    DEFAULT_MBUF_MPOOL_NBUFS];

struct os_mbuf_pool default_mbuf_pool;
struct os_mempool default_mbuf_mpool;

#define SHELL_TASK_PRIO (3)
#define SHELL_MAX_INPUT_LEN     (256)
#define SHELL_TASK_STACK_SIZE (OS_STACK_ALIGN(384))
os_stack_t shell_stack[SHELL_TASK_STACK_SIZE];

int esp_cmd_send(int argc, char **argv);

static struct shell_cmd esp_cli_cmd = {
    .sc_cmd = "esp",
    .sc_cmd_func = esp_cmd_send
};

static void
print_rsp(struct PACKET_CMD *rsp)
{
    int i, j;
    struct ARGS *a;
    char ch;

    console_printf(" - cmd: %u, callback: %lx return: %lx argc: %d\n",
      rsp->cmd, rsp->callback, rsp->_return, rsp->argc);
    a = &rsp->args;
    for (i = 0; i < rsp->argc; i++) {
        console_printf("   - %d ", a->len);
        for (j = 0; j < a->len; j++) {
            ch = ((char *)&a->data)[j];
            console_printf("%x (%c) ", ch, isalnum((int)ch) ? ch : ' ');
            if (j % 8 == 7) {
                console_printf("\n");
                if (a->len > j + 1) {
                    console_printf("        ");
                }
            }
        }
        if (j % 8) {
            console_printf("\n");
        }
        a = (struct ARGS *)((uint8_t *)(a + 1) + a->len);
    }
}

void
wifiCb(struct PACKET_CMD *response)
{
    console_printf("in wifiCb\n");
    print_rsp(response);
}

void
restCb(struct PACKET_CMD *response)
{
    console_printf("in restCb\n");
    print_rsp(response);
}

int
esp_cmd_send(int argc, char **argv)
{
    uint16_t crc;
    int val;
    uint32_t rc;

    if (argc < 2) {
        console_printf("check esp_cmd_send() for usage\n");
        return 0;
    }
    if (!strcmp(argv[1], "wifi")) {
        /*
         * XXX I don't think ESP-Link takes these commands?
         */
        if (argc < 4) {
            console_printf("wrong\n");
            return 0;
        }
        console_printf("connecting SSID: %s pwd %s\n", argv[2], argv[3]);
        crc = esp_request_start(CMD_WIFI_CONNECT, &wifiCb, 0, 2);
        crc = esp_request_cont(crc, argv[2], strlen(argv[2]));
        crc = esp_request_cont(crc, argv[3], strlen(argv[3]));
        esp_request_end(crc);
        esp_wait_return_timo(10000, &rc);
        esp_wait_return_timo(10000, &rc);
        esp_wait_return_timo(10000, &rc);
    } else if (!strcmp(argv[1], "ready")) {
        /*
         * This should be ok.
         */
        val = esp_ready();
        if (val == true) {
            console_printf("ESP ready\n");
        } else {
            console_printf("ESP not ready\n");
        }
    } else if (!strcmp(argv[1], "rest")) {
        /*
         * Try 'esp rest runtime.io /'
         */
        struct esp_rest er;
        uint16_t http_code;
        uint16_t len;
        char data[512];

        if (argc < 4) {
            console_printf("Usage: esp rest <hostname> <path>\n");
            return 0;
        }
        esp_rest_init(&er);
        val = esp_rest_begin(&er, argv[2], 80, 0);
        if (val != true) {
            console_printf("begin fail\n");
            return 0;
        }

        esp_rest_get(&er, argv[3], NULL);

        len = sizeof(data);
        http_code = esp_rest_get_response(&er, data, &len);
        if (http_code == 0) {
            console_printf("get response fail\n");
            return 0;
        }
        console_printf("return code %d\n", http_code);
        console_printf("%s\n", data);
    }
    return 0;
}

void
espduino_test_init(void)
{
    espduino_init(ESPDUINO_UART, ESPDUINO_UART_SPEED);
    shell_cmd_register(&esp_cli_cmd);
}

/**
 * main
 *
 * The main function for the project. This function initializes the os, then
 * starts the OS. We should not return from os start.
 *
 * @return int NOTE: this function should never return!
 */
int
main(int argc, char **argv)
{
    int rc;

#ifdef ARCH_sim
    mcu_sim_parse_args(argc, argv);
#endif

    os_init();

    rc = os_mempool_init(&default_mbuf_mpool, DEFAULT_MBUF_MPOOL_NBUFS,
            DEFAULT_MBUF_MPOOL_BUF_LEN, default_mbuf_mpool_data,
            "default_mbuf_data");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&default_mbuf_pool, &default_mbuf_mpool,
            DEFAULT_MBUF_MPOOL_BUF_LEN, DEFAULT_MBUF_MPOOL_NBUFS);
    assert(rc == 0);

    rc = os_msys_register(&default_mbuf_pool);
    assert(rc == 0);

    espduino_test_init();
    console_printf("\nEspduino testing\n");

    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return rc;
}

