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
#include <hal/hal_adc_int.h>
#include <mcu/hal_adc.h>
#include <hal/hal_pwm_int.h>
#include <mcu/hal_pwm.h>
#include <mcu/hal_dac.h>

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

/* the default arduino configuration uses 3v3 volts as the reference 
 * voltage.  This is equivalent to using VCC/2 as the internal 
 * reference with a divide by 2 in the pre-stage */
static const struct samd21_adc_config default_cfg = 
{
    .gain = SAMD21_GAIN_DIV2,
    .volt = SAMD21_ADC_REFERENCE_INTVCC1,       
    .voltage_mvolts = 3300,
    .resolution_bits = SAMD21_RESOLUTION_10_BITS,
};

/* if you wanted to enable AREF on some channels, you would use this config,
 * of course replacing the voltage to what you are hooking it to and 
 * set the resolutionand gain for what you need  */
static const struct samd21_adc_config aref_cfg = 
{
    .gain = SAMD21_GAIN_1X,
    .volt = SAMD21_ADC_REFERENCE_AREFA,       
    .voltage_mvolts = 3300,
    .resolution_bits = SAMD21_RESOLUTION_12_BITS,
};

extern struct hal_adc *
bsp_get_hal_adc(enum system_device_id sysid) {       
    switch(sysid) {
        case ARDUINO_ZERO_A0: 
            return samd21_adc_create(SAMD21_ANALOG_0, &default_cfg);
        case ARDUINO_ZERO_A1:
            return samd21_adc_create(SAMD21_ANALOG_2, &default_cfg);
        case ARDUINO_ZERO_A2: 
            return samd21_adc_create(SAMD21_ANALOG_3, &default_cfg);
        case ARDUINO_ZERO_A3:
            return samd21_adc_create(SAMD21_ANALOG_4, &default_cfg);
        case ARDUINO_ZERO_A4: 
            return samd21_adc_create(SAMD21_ANALOG_5, &default_cfg);
        case ARDUINO_ZERO_A5: 
            return samd21_adc_create(SAMD21_ANALOG_10, &default_cfg);
        default:
            break;
    }
    return NULL;    
}

/* the GCLK going to the TCs on arduino is 8 Mhz.
 * Dont pre-scale since a 16-bit timer would wrap
 * at 122 Hz which is good for flicker free LED.
 * if you attach other stuff, you may want a lower 
 * duty cycle */
static const struct samd21_pwm_tc_config tc_cfg = {
    .prescalar = SAMD_TC_CLOCK_PRESCALER_DIV1,
    .clock_freq = 8000000,
    /* TODO allow us to specify a clock source */
};

/* the GCLK going to the TCs on arduino is 8 Mhz.
 * Dont pre-scale since a 24-bit timer would wrap
 * at ~0.47 Hz which is good for most wearable timing
 * if you attach other stuff, you may want a lower 
 * duty cycle */
static const struct samd21_pwm_tcc_config tcc_cfg = {
    .prescalar = SAMD_TC_CLOCK_PRESCALER_DIV1,
    .clock_freq = 8000000,
    /* TODO allow us to specify a clock source */
};

extern struct hal_pwm*
bsp_get_hal_pwm_driver(enum system_device_id sysid) {
    struct hal_pwm *ppwm = NULL;
    
    switch(sysid) {
        case ARDUINO_ZERO_D0:            
            ppwm = samd21_pwm_tcc_create(SAMD_TCC_DEV_0, 3, 11, &tcc_cfg);
            break;
        case ARDUINO_ZERO_D1:            
            ppwm = samd21_pwm_tcc_create(SAMD_TCC_DEV_0, 2, 10, &tcc_cfg);
            break;
        case ARDUINO_ZERO_D2:            
            ppwm = samd21_pwm_tcc_create(SAMD_TCC_DEV_0, 0,  8, &tcc_cfg);
            break;
        case ARDUINO_ZERO_D3:            
            ppwm = samd21_pwm_tcc_create(SAMD_TCC_DEV_0, 1,  9, &tcc_cfg);
            break;
            /* These are the classic PWM pins on Arduino. NOTE some
             * are driven by different timer drivers  */
        case ARDUINO_ZERO_D8:            
            ppwm = samd21_pwm_tcc_create(SAMD_TCC_DEV_1, 0,  6, &tcc_cfg);
            break;            
        case ARDUINO_ZERO_D9:
            ppwm = samd21_pwm_tcc_create(SAMD_TCC_DEV_1, 1,  7, &tcc_cfg);
            break;
        case ARDUINO_ZERO_D10:
            ppwm =  samd21_pwm_tc_create(SAMD_TC_DEV_3,  0, 18,  &tc_cfg);
            break;            
        case ARDUINO_ZERO_D11:
            ppwm = samd21_pwm_tcc_create(SAMD_TCC_DEV_2, 0, 16, &tcc_cfg);            
            break;            
        case ARDUINO_ZERO_D12:
            ppwm =  samd21_pwm_tc_create(SAMD_TC_DEV_3,  1, 19,  &tc_cfg);
            break;
        case ARDUINO_ZERO_D13:
            ppwm = samd21_pwm_tcc_create(SAMD_TCC_DEV_2, 1, 17, &tcc_cfg);            
            break;            
            /* its possible to configure some analog line for digital as well */
        case ARDUINO_ZERO_A1:
            ppwm =  samd21_pwm_tc_create(SAMD_TC_DEV_4,  0, 40,  &tc_cfg);
            break;
        case ARDUINO_ZERO_A2:
            ppwm =  samd21_pwm_tc_create(SAMD_TC_DEV_4,  1, 41,  &tc_cfg);
            break;
        case ARDUINO_ZERO_A3:
            ppwm = samd21_pwm_tcc_create(SAMD_TCC_DEV_0, 0,  4, &tcc_cfg);
            break;
        case ARDUINO_ZERO_A4:
            ppwm = samd21_pwm_tcc_create(SAMD_TCC_DEV_0, 1,  5, &tcc_cfg);
            break;

        default:
            break;
    }
    return ppwm;
}

static const struct samd21_dac_config dac_cfg = 
{
    .dac_reference_voltage_mvolts = 3300,
    .reference = SAMD_DAC_REFERENCE_AVCC,
};

extern struct hal_dac*
bsp_get_hal_dac_driver(enum system_device_id sysid) 
{
    struct hal_dac *pdac = NULL;
    
    switch (sysid) {
        case ARDUINO_ZERO_A0:
            pdac = samd21_dac_create(&dac_cfg); 
            break;
        default:
            break;
    }
    return pdac;
}
