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
#include <hal/hal_spi.h>
#include <hal/hal_i2c.h>
#include <hal/hal_bsp.h>
#include <mcu/hal_spi.h>
#include <mcu/hal_i2c.h>

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

/* configure the SPI port for arduino external spi */
struct samd21_spi_config icsp_spi_config = {
    .dipo = 0,  /* sends MISO to PAD 0 */
    .dopo = 1,  /* send CLK to PAD 3 and MOSI to PAD 2 */
    .pad0_pinmux = PINMUX_PA12D_SERCOM4_PAD0,       /* MISO */
    .pad3_pinmux = PINMUX_PB11D_SERCOM4_PAD3,       /* SCK */
    .pad2_pinmux = PINMUX_PB10D_SERCOM4_PAD2,       /* MOSI */
};

/* NOTE using this will overwrite the debug interface */
struct samd21_spi_config alt_spi_config = {
    .dipo = 3,  /* sends MISO to PAD 3 */
    .dopo = 0,  /* send CLK to PAD 1 and MOSI to PAD 0 */
    .pad0_pinmux = PINMUX_PA04D_SERCOM0_PAD0,       /* MOSI */
    .pad1_pinmux = PINMUX_PA05D_SERCOM0_PAD1,       /* SCK */
    .pad2_pinmux = PINMUX_PA06D_SERCOM0_PAD2,       /* SCK */
    .pad3_pinmux = PINMUX_PA07D_SERCOM0_PAD3,       /* MISO */
};

struct samd21_i2c_config i2c_config = {
    .pad0_pinmux = PINMUX_PA22D_SERCOM5_PAD0,
    .pad1_pinmux = PINMUX_PA23D_SERCOM5_PAD1,
};

int
bsp_hal_init(void)
{
    int rc;

    rc = hal_spi_init(ARDUINO_ZERO_SPI_ICSP, &icsp_spi_config);
    if (rc != 0) {
        goto err;
    }

    rc = hal_spi_init(ARDUINO_ZERO_SPI_ALT, &alt_spi_config);
    if (rc != 0) {
        goto err;
    }

    rc = hal_i2c_init(5, &i2c_config);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

static const struct samd21_uart_config uart_cfgs[] = {
    [0] = {
        .suc_sercom = SERCOM2,
        .suc_mux_setting = USART_RX_3_TX_2_XCK_3,
        .suc_generator_source = GCLK_GENERATOR_0,
        .suc_sample_rate = USART_SAMPLE_RATE_16X_ARITHMETIC,
        .suc_sample_adjustment = USART_SAMPLE_ADJUSTMENT_7_8_9,
        .suc_pad0 = 0,
        .suc_pad1 = 0,
        .suc_pad2 = PINMUX_PA10D_SERCOM2_PAD2,
        .suc_pad3 = PINMUX_PA11D_SERCOM2_PAD3
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
