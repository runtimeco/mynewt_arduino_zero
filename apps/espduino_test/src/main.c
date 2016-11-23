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
#include <syscfg/syscfg.h>
#include <sysinit/sysinit.h>
#include <sysflash/sysflash.h>

#include <os/os.h>
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash.h>
#include <console/console.h>
#include <hal/hal_uart.h>
#include <hal/hal_system.h>

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

#define WORKER_PRIO       (3)
#define WORKER_STACK_SIZE (OS_STACK_ALIGN(1024))
os_stack_t worker_stack[WORKER_STACK_SIZE];
static struct os_eventq worker_evq;
static struct os_task worker_task;



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
    int val;

    if (argc < 2) {
        console_printf("check esp_cmd_send() for usage\n");
        return 0;
    }
    if (!strcmp(argv[1], "ready")) {
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
    } else {
        console_printf("Invalid command\n");
        console_printf("esp [ready|rest <hostname> <path>]\n");
    }
    return 0;
}

void
espduino_test_init(void)
{
    espduino_init(ESPDUINO_UART, ESPDUINO_UART_SPEED);
    shell_cmd_register(&esp_cli_cmd);
}

static void
worker_func(void *unused)
{
    while (1) {
        os_eventq_run(&worker_evq);
    }
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

    sysinit();

    /* Initialize eventq */
    os_eventq_init(&worker_evq);

    /* Create the bleprph task.  All application logic and NimBLE host
     * operations are performed in this task.
     */
    os_task_init(&worker_task, "worker", worker_func,
                 NULL, WORKER_PRIO, OS_WAIT_FOREVER,
                 worker_stack, WORKER_STACK_SIZE);
    os_eventq_dflt_set(&worker_evq);

    espduino_test_init();
    console_printf("\nEspduino testing\n");

    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return rc;
}

