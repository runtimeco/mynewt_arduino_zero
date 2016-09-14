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

#include <assert.h>
#include <errno.h>

#include "syscfg/syscfg.h"
#include "hal/hal_spi.h"
#include "compiler.h"
#include "port.h"
#include "mcu/hal_spi.h"
#include "sercom.h"
#include "spi.h"
#include "spi_interrupt.h"
#include "mcu/samd21.h"


#define SAMD21_SPI_FLAG_ENABLED     (0x1)
#define SAMD21_SPI_FLAG_9BIT        (0x2)

struct samd21_hal_spi {
    struct spi_module               module;
    struct hal_spi_settings spi_cfg; /* Slave and master */
    const struct samd21_spi_config *pconfig;
    uint8_t                         flags;
};

#define HAL_SAMD21_SPI_MAX (6)

#if MYNEWT_VAL(SPI_0)
static struct samd21_hal_spi samd21_hal_spi0;
#endif
#if MYNEWT_VAL(SPI_1)
static struct samd21_hal_spi samd21_hal_spi1;
#endif
#if MYNEWT_VAL(SPI_2)
static struct samd21_hal_spi samd21_hal_spi2;
#endif
#if MYNEWT_VAL(SPI_3)
static struct samd21_hal_spi samd21_hal_spi3;
#endif
#if MYNEWT_VAL(SPI_4)
static struct samd21_hal_spi samd21_hal_spi4;
#endif
#if MYNEWT_VAL(SPI_5)
static struct samd21_hal_spi samd21_hal_spi5;
#endif

static struct samd21_hal_spi *samd21_hal_spis[HAL_SAMD21_SPI_MAX] = {
#if MYNEWT_VAL(SPI_0)
    &samd21_hal_spi0,
#else
    NULL,
#endif
#if MYNEWT_VAL(SPI_1)
    &samd21_hal_spi1,
#else
    NULL,
#endif
#if MYNEWT_VAL(SPI_2)
    &samd21_hal_spi2,
#else
    NULL,
#endif
#if MYNEWT_VAL(SPI_3)
    &samd21_hal_spi3,
#else
    NULL,
#endif
#if MYNEWT_VAL(SPI_4)
    &samd21_hal_spi4,
#else
    NULL,
#endif
#if MYNEWT_VAL(SPI_5)
    &samd21_hal_spi5,
#else
    NULL,
#endif
};

static struct samd21_hal_spi *
samd21_hal_spi_resolve(int spi_num)
{
    struct samd21_hal_spi *spi;

    if (spi_num >= HAL_SAMD21_SPI_MAX) {
        spi = NULL;
    } else {
        spi = samd21_hal_spis[spi_num];
    }

    return spi;
}

int
hal_spi_init(int spi_num, void *cfg, uint8_t spi_type)
{
    struct samd21_hal_spi *spi;

    if (cfg == NULL) {
        return EINVAL;
    }

    /* Slave currently unsupported. */
    if (spi_type != HAL_SPI_TYPE_MASTER) {
        return EINVAL;
    }

    spi = samd21_hal_spi_resolve(spi_num);
    if (spi == NULL) {
        return EINVAL;
    }

    spi->pconfig = cfg;

    switch (spi_num) {
        case 0:
            spi->module.hw = SERCOM0;
            break;
        case 1:
            spi->module.hw = SERCOM1;
            break;
        case 2:
            spi->module.hw = SERCOM2;
            break;
        case 3:
            spi->module.hw = SERCOM3;
            break;
        case 4:
            spi->module.hw = SERCOM4;
            break;
        case 5:
            spi->module.hw = SERCOM5;
            break;
        default:
            return EINVAL;
    }

    return 0;
}

static int
samd21_spi_config_master(struct samd21_hal_spi *spi,
                         struct hal_spi_settings *settings)
{
    struct spi_config cfg;
    uint32_t ctrla;
    int rc;

    spi_get_config_defaults(&cfg);

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
    switch (settings->word_size) {
        case HAL_SPI_WORD_SIZE_8BIT:
            cfg.character_size = SPI_CHARACTER_SIZE_8BIT;
            spi->flags &= ~SAMD21_SPI_FLAG_9BIT;
            break;
        case HAL_SPI_WORD_SIZE_9BIT:
            cfg.character_size = SPI_CHARACTER_SIZE_9BIT;
            spi->flags |= SAMD21_SPI_FLAG_9BIT;
            break;
        default:
            return EINVAL;
    }

    switch (settings->data_order) {
        case HAL_SPI_LSB_FIRST:
            cfg.data_order = SPI_DATA_ORDER_LSB;
            break;
        case HAL_SPI_MSB_FIRST:
            cfg.data_order = SPI_DATA_ORDER_MSB;
            break;
        default:
            return EINVAL;
    }

    switch (settings->data_mode) {
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
            return EINVAL;
    }

    cfg.mode_specific.master.baudrate = settings->baudrate;

    rc = spi_init(&spi->module, spi->module.hw, &cfg);
    if (rc != STATUS_OK) {
        return EIO;
    }

    return 0;
}

int
hal_spi_config(int spi_num, struct hal_spi_settings *settings)
{
    struct samd21_hal_spi *spi;
    int rc;

    spi = samd21_hal_spi_resolve(spi_num);
    if (spi == NULL) {
        return EINVAL;
    }

    /* Slave currently unsupported. */
    if (settings->spi_type != HAL_SPI_TYPE_MASTER) {
        return EINVAL;
    }

    spi->spi_cfg = *settings;

    rc = samd21_spi_config_master(spi, settings);
    return rc;
}

int
hal_spi_enable(int spi_num)
{
    struct samd21_hal_spi *spi;

    spi = samd21_hal_spi_resolve(spi_num);
    if (spi == NULL) {
        return EINVAL;
    }

    spi_enable(&spi->module);
    spi->flags |= SAMD21_SPI_FLAG_ENABLED;

    return 0;
}

int
hal_spi_disable(int spi_num)
{
    struct samd21_hal_spi *spi;

    spi = samd21_hal_spi_resolve(spi_num);
    if (spi == NULL) {
        return EINVAL;
    }

    spi_disable(&spi->module);
    spi->flags &= ~SAMD21_SPI_FLAG_ENABLED;

    return 0;
}

uint16_t
hal_spi_tx_val(int spi_num, uint16_t tx)
{
    struct samd21_hal_spi *spi;
    enum status_code status;
    uint16_t rx;

    spi = samd21_hal_spi_resolve(spi_num);
    assert(spi != NULL);

    status = spi_transceive_wait(&spi->module, tx, &rx);
    assert(status == 0);

    return rx;
}

int
hal_spi_set_txrx_cb(int spi_num, hal_spi_txrx_cb txrx_cb, void *arg)
{
    struct samd21_hal_spi *spi;

    spi = samd21_hal_spi_resolve(spi_num);
    if (spi == NULL) {
        return EINVAL;
    }

    if (!(spi->flags & SAMD21_SPI_FLAG_ENABLED)) {
        return EACCES;
    }

    spi->spi_cfg.txrx_cb_func = txrx_cb;
    spi->spi_cfg.txrx_cb_arg = arg;

    return 0;
}

int
hal_spi_txrx(int spi_num, void *txbuf, void *rxbuf, int len)
{
    struct samd21_hal_spi *spi;
    enum status_code status;

    spi = samd21_hal_spi_resolve(spi_num);
    if (spi == NULL) {
        return EINVAL;
    }

    if (len <= 0 || len > UINT16_MAX) {
        return EINVAL;
    }

    /* Slave currently unsupported. */
    assert(spi->spi_cfg.spi_type == HAL_SPI_TYPE_MASTER);

    status = spi_transceive_buffer_wait(&spi->module, txbuf, rxbuf, len);
    switch (status) {
    case STATUS_OK:
        return 0;

    case STATUS_ERR_INVALID_ARG:
        return EINVAL;

    case STATUS_ERR_TIMEOUT:
    case STATUS_ERR_DENIED:
    case STATUS_ERR_OVERFLOW:
    default:
        return EIO;
    }
}

int
hal_spi_slave_set_def_tx_val(int spi_num, uint16_t val)
{
    struct samd21_hal_spi *spi;
    enum status_code status;

    spi = samd21_hal_spi_resolve(spi_num);
    if (spi == NULL) {
        return EINVAL;
    }

    if (!spi_is_ready_to_write(&spi->module)) {
        return EACCES;
    }

    status = spi_write(&spi->module, val);
    if (status != STATUS_OK) {
        return EIO;
    }

    return 0;
}

int
hal_spi_abort(int spi_num)
{
    struct samd21_hal_spi *spi;

    spi = samd21_hal_spi_resolve(spi_num);
    if (spi == NULL) {
        return EINVAL;
    }

    spi_abort_job(&spi->module);
    return 0;
}
