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
#include <assert.h>

#include <os/os.h>
#include <os/os_cputime.h>
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include "winc1500/bsp/nm_bsp.h"

#include "winc1500_priv.h"

void
nm_bsp_sleep(uint32 msec)
{
    if (os_started()) {
        os_time_delay((msec * OS_TICKS_PER_SEC) / 1000);
    } else {
        os_cputime_delay_usecs(msec * 1000);
    }
}

/*
 * Register interrupt handler
 */
void
nm_bsp_register_isr(tpfNmBspIsr isr)
{
    int rc;
    static uint8_t reg_done;

    if (!reg_done) {
        rc = hal_gpio_irq_init(WINC1500_PIN_IRQ, (hal_gpio_irq_handler_t)isr,
                               NULL, HAL_GPIO_TRIG_FALLING, HAL_GPIO_PULL_UP);
        assert(rc == 0);
        reg_done = 1;
    }
    hal_gpio_irq_enable(WINC1500_PIN_IRQ);
}

/*
 * Enable/disable interrupt
 */
void
nm_bsp_interrupt_ctrl(uint8 enable)
{
    if (enable) {
        hal_gpio_irq_enable(WINC1500_PIN_IRQ);
    } else {
        hal_gpio_irq_disable(WINC1500_PIN_IRQ);
    }
}

/*
 * Reset NMC1500 SoC by setting CHIP_EN and RESET_N signals low,
 * CHIP_EN high then RESET_N high
 */
void
nm_bsp_reset(void)
{
#ifdef WINC1500_PIN_ENABLE
    hal_gpio_write(WINC1500_PIN_ENABLE, 0);
#endif
    hal_gpio_write(WINC1500_PIN_RESET, 0);
    nm_bsp_sleep(100); /* 100ms */
#ifdef WINC1500_PIN_ENABLE
    hal_gpio_write(WINC1500_PIN_ENABLE, 1);
    nm_bsp_sleep(10); /* 10ms */
#endif
    hal_gpio_write(WINC1500_PIN_RESET, 1);
    nm_bsp_sleep(10); /* 10ms */
}
