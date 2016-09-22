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
#include <stdint.h>
#include <assert.h>
#include "bsp/cmsis_nvic.h"
#include "hal/hal_cputime.h"

#include "sam0/drivers/tc/tc.h"
#include "sam0/utils/status_codes.h"
#include "common/utils/interrupt/interrupt_sam_nvic.h"

struct tc_module g_cputime_timer;

/* XXX:
 *  - Must determine how to set priority of cpu timer interrupt
 *  - Determine if we should use a mutex as opposed to disabling interrupts
 *  - Should I use macro for compare channel?
 *  - Sync to OSTIME.
 */

void
cputime_disable_ocmp(void)
{
    /* XXX: implement */
}

/**
 * cputime init
 *
 * Initialize the cputime module. This must be called after os_init is called
 * and before any other timer API are used. This should be called only once
 * and should be called before the hardware timer is used.
 *
 * @param clock_freq The desired cputime frequency, in hertz (Hz).
 *
 * @return int 0 on success; -1 on error.
 */
int
cputime_hw_init(uint32_t clock_freq)
{
    enum status_code rc;
    struct system_gclk_gen_config gcfg;

    struct tc_config cfg;
    tc_get_config_defaults(&cfg);

#if defined(HAL_CPUTIME_1MHZ)
    if (clock_freq != 1000000) {
        return -1;
    }
#endif

    if(clock_freq > 8000000) {
        return -1;
    }

    /* set up gclk generator 1 to source this timer */
    gcfg.division_factor = 1;
    gcfg.high_when_disabled = false;
    gcfg.output_enable = true;
    gcfg.run_in_standby = true;
    gcfg.source_clock = GCLK_SOURCE_OSC8M;
    system_gclk_gen_set_config(GCLK_GENERATOR_1, &gcfg);

    cfg.counter_size = TC_COUNTER_SIZE_32BIT;
    cfg.clock_source = GCLK_GENERATOR_1;
    cfg.clock_prescaler = TC_CLOCK_PRESCALER_DIV1;

    g_cputime.ticks_per_usec = 8;

    rc = tc_init(&g_cputime_timer, TC4, &cfg);
    switch (rc) {
    case STATUS_OK:
        tc_enable(&g_cputime_timer);
        return 0;

    case STATUS_ERR_DENIED:
        /* Timer already enabled. */
        return 0;

    default:
        return -1;
    }
}

/**
 * cputime get64
 *
 * Returns cputime as a 64-bit number.
 *
 * @return uint64_t The 64-bit representation of cputime.
 */
uint64_t
cputime_get64(void)
{
    uint32_t high;
    uint32_t low;
    uint64_t cpu_time;

    cpu_irq_enter_critical();
    high = g_cputime.cputime_high;
    low = cputime_get32();
    if (/* TODO overflow */ 0 ) {
        ++high;
        low = cputime_get32();
    }
    cpu_irq_leave_critical();

    cpu_time = ((uint64_t)high << 32) | low;

    return cpu_time;
}

/**
 * cputime get32
 *
 * Returns the low 32 bits of cputime.
 *
 * @return uint32_t The lower 32 bits of cputime
 */
uint32_t
cputime_get32(void)
{
    uint32_t cpu_time;

    cpu_time = tc_get_count_value(&g_cputime_timer);

    return cpu_time;
}
