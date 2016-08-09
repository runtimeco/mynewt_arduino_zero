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
#include <stddef.h>
#include "mcu/samd21.h"
#include "bsp/bsp.h"
#include <bsp/bsp_sysid.h>
#include <hal/hal_bsp.h>
#include <hal/hal_spi.h>
#include <mcu/hal_spi.h>

/*
 * hw/mcu/atmel/samd21xx/src/sam0/drivers/sercom/usart/usart.h
 */
#include <usart.h>
#include <mcu/hal_uart.h>

const struct hal_flash *
bsp_flash_dev(uint8_t id)
{
    /*
     * Internal flash mapped to id 0.
     */
    if (id != 0) {
        return NULL;
    }
    return &samd21_flash_dev;
}

/*
 * What memory to include in coredump.
 */
static const struct bsp_mem_dump dump_cfg[] = {
    [0] = {
	.bmd_start = &_ram_start,
        .bmd_size = RAM_SIZE
    }
};

const struct bsp_mem_dump *
bsp_core_dump(int *area_cnt)
{
    *area_cnt = sizeof(dump_cfg) / sizeof(dump_cfg[0]);
    return dump_cfg;
}

static struct samd21_spi_config winc1500_spi_cfg = {
    .dipo = 3,
    .dopo = 0,
    .pad0_pinmux = PINMUX_PA12C_SERCOM2_PAD0,   /* MISO */
    .pad1_pinmux = PINMUX_PA13C_SERCOM2_PAD1,   /* SCK */
    .pad3_pinmux = PINMUX_PA15C_SERCOM2_PAD3    /* MISO */
};


int
bsp_hal_init(void)
{
    int rc;

    rc = hal_spi_init(WINC1500_SPI_PORT, &winc1500_spi_cfg);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

static const struct samd21_uart_config uart_cfgs[] = {
    [0] = {
        .suc_sercom = SERCOM5,
        .suc_mux_setting = USART_RX_3_TX_2_XCK_3,
        .suc_generator_source = GCLK_GENERATOR_0,
        .suc_sample_rate = USART_SAMPLE_RATE_16X_ARITHMETIC,
        .suc_sample_adjustment = USART_SAMPLE_ADJUSTMENT_7_8_9,
        .suc_pad0 = 0,
        .suc_pad1 = 0,
        .suc_pad2 = PINMUX_PB22D_SERCOM5_PAD2,
        .suc_pad3 = PINMUX_PB23D_SERCOM5_PAD3
    }
};

const struct samd21_uart_config *
bsp_uart_config(int port)
{
    if (port < sizeof(uart_cfgs) / sizeof(uart_cfgs[0])) {
        return &uart_cfgs[port];
    }
    return NULL;
}
