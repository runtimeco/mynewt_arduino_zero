/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <hal/hal_spi.h>
#include <assert.h>
#include <compiler.h>
#include "port.h"
#include <mcu/hal_spi.h>
#include <sercom.h>
#include <spi.h>
#include <mcu/samd21.h>
#include <errno.h>


#define SAMD21_SPI_FLAG_ENABLED     (0x1)
#define SAMD21_SPI_FLAG_9BIT        (0x2)

struct samd21_spi_state {
    struct spi_module               module;
    Sercom *                        hw;
    const struct samd21_spi_config *pconfig;
    uint8_t                         flags;
};

#define HAL_SAMD21_SPI_MAX (6)

struct samd21_spi_state *samd21_hal_spis[HAL_SAMD21_SPI_MAX];

#define SAMD21_SPI_RESOLVE(__n, __v)        \
    if ((__n) >= HAL_SAMD21_SPI_MAX) {       \
        rc = EINVAL;                        \
        goto err;                           \
    }                                       \
    (__v) = samd21_hal_spis[(__n)];


int
hal_spi_init(uint8_t spi_num, void *cfg)
{
    struct samd21_spi_state *spi;
    int rc;

    assert(cfg != NULL);

    SAMD21_SPI_RESOLVE(spi_num, spi);

    spi->pconfig = (struct samd21_spi_config *) cfg;

    switch (spi_num) {
        case 0:
            spi->hw = SERCOM0;
            break;
        case 1:
            spi->hw = SERCOM1;
            break;
        case 2:
            spi->hw = SERCOM2;
            break;
        case 3:
            spi->hw = SERCOM3;
            break;
        case 4:
            spi->hw = SERCOM4;
            break;
        case 5:
            spi->hw = SERCOM5;
            break;
        default:
            rc = EINVAL;
            goto err;
    }

    return (0);
err:
    return (rc);
}


int
samd21_spi_config(uint8_t spi_num, struct hal_spi_settings *psettings)
{
    struct samd21_spi_state *spi;
    struct spi_config cfg;
    uint32_t ctrla;
    int rc;

    SAMD21_SPI_RESOLVE(spi_num, spi);

    if (spi->flags & SAMD21_SPI_FLAG_ENABLED) {
        spi_disable(&spi->module);
        spi->flags &= ~SAMD21_SPI_FLAG_ENABLED;
    }

    ctrla = (spi->pconfig->dopo << SERCOM_SPI_CTRLA_DOPO_Pos) |
            (spi->pconfig->dipo << SERCOM_SPI_CTRLA_DIPO_Pos);

    cfg.pinmux_pad0 = spi->pconfig->pad0_pinmux;
    cfg.pinmux_pad1 = spi->pconfig->pad1_pinmux;
    cfg.pinmux_pad2 = spi->pconfig->pad2_pinmux;
    cfg.pinmux_pad3 = spi->pconfig->pad3_pinmux;
    cfg.mux_setting = ctrla;

    /* apply the hal_settings */
    switch (psettings->word_size) {
        case HAL_SPI_WORD_SIZE_8BIT:
            cfg.character_size = SPI_CHARACTER_SIZE_8BIT;
            spi->flags &= ~SAMD21_SPI_FLAG_9BIT;
            break;
        case HAL_SPI_WORD_SIZE_9BIT:
            cfg.character_size = SPI_CHARACTER_SIZE_9BIT;
            spi->flags |= SAMD21_SPI_FLAG_9BIT;
            break;
        default:
            rc = -1;
            goto err;
    }

    switch (psettings->data_order) {
        case HAL_SPI_LSB_FIRST:
            cfg.data_order = SPI_DATA_ORDER_LSB;
            break;
        case HAL_SPI_MSB_FIRST:
            cfg.data_order = SPI_DATA_ORDER_MSB;
            break;
        default:
            rc = -2;
            goto err;
    }

    switch (psettings->data_mode) {
        case HAL_SPI_MODE0:
            cfg.transfer_mode = SPI_TRANSFER_MODE_0;
            break;
        case HAL_SPI_MODE1:
            cfg.transfer_mode = SPI_TRANSFER_MODE_1;
            break;
        case HAL_SPI_MODE2:
            cfg.transfer_mode = SPI_TRANSFER_MODE_2;
            break;
        case HAL_SPI_MODE3:
            cfg.transfer_mode = SPI_TRANSFER_MODE_3;
            break;
        default:
            rc = -3;
            goto err;
    }

    cfg.mode_specific.master.baudrate = psettings->baudrate;

    rc = spi_init(&spi->module, spi->hw, &cfg);
    if (STATUS_OK != rc) {
        return (-4);
    }

    spi_enable(&spi->module);

    spi->flags |= SAMD21_SPI_FLAG_ENABLED;

    return (0);
err:
    return (rc);
}

int
samd21_spi_transfer(uint8_t spi_num, uint16_t tx)
{
    struct samd21_spi_state *spi;
    enum status_code status;
    uint8_t tx_buf[2];
    uint8_t rx_buf[2];
    int read_val;
    int rc;

    SAMD21_SPI_RESOLVE(spi_num, spi);

    if (spi->flags & SAMD21_SPI_FLAG_9BIT) {
        tx_buf[1] = tx >> 8;
        tx_buf[0] = tx & 0xff;

        status = spi_transceive_buffer_wait(&spi->module, tx_buf, rx_buf, 2);
        read_val =  (int) rx_buf[0] << 8;
        read_val |= (int) rx_buf[1];
    } else {
        tx_buf[0] = (uint8_t) tx;

        status = spi_transceive_buffer_wait(&spi->module, &tx_buf[0], &rx_buf[0],
                1);
        read_val = (int) rx_buf[0];
    }

    if (status != STATUS_OK) {
        rc = -2;
        goto err;
    }

    return (read_val);
err:
    return (rc);
}
