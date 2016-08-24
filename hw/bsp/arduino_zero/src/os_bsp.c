/**
 *
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
#include <sys/types.h>
#include <assert.h>

#include <hal/hal_bsp.h>
#include <hal/flash_map.h>

#include <os/os_dev.h>
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>

#include "bsp/bsp.h"

void _close(int fd);

static struct flash_area arduino_zero_flash_areas[] = {
    [FLASH_AREA_BOOTLOADER] = {
        .fa_flash_id = 0,       /* internal flash */
        .fa_off = 0x00000000,   /* beginning */
        .fa_size = (48 * 1024)
    },
    [FLASH_AREA_IMAGE_0] = {
        .fa_flash_id = 0,
        .fa_off = 0x0000c000,
        .fa_size = (96 * 1024)
    },
    [FLASH_AREA_IMAGE_1] = {
        .fa_flash_id = 0,
        .fa_off = 0x00024000,
        .fa_size = (96 * 1024)
    },
    [FLASH_AREA_IMAGE_SCRATCH] = {
        .fa_flash_id = 0,
        .fa_off = 0x0003c000,
        .fa_size = (7 * 1024)
    },
    [FLASH_AREA_NFFS] = {
        .fa_flash_id = 0,
        .fa_off = 0x0003e000,
        .fa_size = (8 * 1024)
    },
    [FLASH_AREA_REBOOT_LOG] = {
        .fa_flash_id = 0,
        .fa_off = 0x0003dc00,
        .fa_size = (1 * 1024)
    },
};

/* XXX should not be declared here */
extern const struct samd21_uart_config *bsp_uart_config(int port);
static struct uart_dev hal_uart0;

int
bsp_imgr_current_slot(void)
{
    return FLASH_AREA_IMAGE_0;
}

void
bsp_init(void)
{
    int rc;

    /*
     * XXX these references are here to keep the functions in for libc to find.
     */
    _sbrk(0);
    _close(0);
    flash_area_init(arduino_zero_flash_areas,
      sizeof(arduino_zero_flash_areas) / sizeof(arduino_zero_flash_areas[0]));

    rc = os_dev_create((struct os_dev *) &hal_uart0, CONSOLE_UART,
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)bsp_uart_config(0));
    assert(rc == 0);
}
