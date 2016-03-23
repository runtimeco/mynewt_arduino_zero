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
