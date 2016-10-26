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
#ifdef CONF_WINC_USE_SPI
#include <assert.h>

#include <os/os.h>
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include <hal/hal_spi.h>
#include "winc1500/bsp/nm_bsp.h"
#include "winc1500/common/nm_common.h"
#include "winc1500/bus_wrapper/nm_bus_wrapper.h"

#include "winc1500_priv.h"

#define NM_BUS_MAX_TRX_SZ       256

tstrNmBusCapabilities egstrNmBusCapabilities = {
        NM_BUS_MAX_TRX_SZ
};

int winc1500_spi_inited;

sint8
nm_bus_init(void *pvinit)
{
    struct hal_spi_settings cfg = { 0 };

    /*
     * Add code to configure spi.
     */
    if (!winc1500_spi_inited) {
        if (hal_gpio_init_out(WINC1500_SPI_SSN, 1)) {
            return M2M_ERR_BUS_FAIL;
        }
        cfg.data_mode = HAL_SPI_MODE0;
        cfg.data_order = HAL_SPI_MSB_FIRST;
        cfg.word_size = HAL_SPI_WORD_SIZE_8BIT;
        cfg.baudrate = WINC1500_SPI_SPEED;

        if (hal_spi_config(BSP_WINC1500_SPI_PORT, &cfg)) {
            return M2M_ERR_BUS_FAIL;
        }
        winc1500_spi_inited = 1;
        if (hal_spi_enable(BSP_WINC1500_SPI_PORT)) {
            return M2M_ERR_BUS_FAIL;
        }
    }
    nm_bsp_reset();
    nm_bsp_sleep(1);

    return M2M_SUCCESS;
}

sint8
nm_bus_deinit(void)
{
    /*
     * Disable SPI.
     */
    return M2M_SUCCESS;
}

static sint8
nm_spi_rw(uint8 *pu8Mosi, uint8 *pu8Miso, uint16 u16Sz)
{
    int rc = M2M_SUCCESS;
    uint8_t tx = 0;
    uint8_t rx;

    /* chip select */
    hal_gpio_write(WINC1500_SPI_SSN, 0);
    while (u16Sz) {
        if (pu8Mosi) {
            tx = *pu8Mosi;
            pu8Mosi++;
        }
        //rx = hal_spi_tx_val(BSP_WINC1500_SPI_PORT, tx);
        rc = hal_spi_txrx(BSP_WINC1500_SPI_PORT, &tx, &rx, 1);
        if (rc != 0) {
            rc = M2M_ERR_BUS_FAIL;
            break;
        }
        if (pu8Miso) {
            *pu8Miso = rx;
            pu8Miso++;
        }
        u16Sz--;
    }

    /* chip deselect */
    hal_gpio_write(WINC1500_SPI_SSN, 1);

    return rc;
}

sint8
nm_bus_ioctl(uint8 cmd, void *arg)
{
    tstrNmSpiRw *param;

    if (cmd != NM_BUS_IOCTL_RW) {
        return -1;
    }
    param = (tstrNmSpiRw *)arg;

    return nm_spi_rw(param->pu8InBuf, param->pu8OutBuf, param->u16Sz);
}
#endif
