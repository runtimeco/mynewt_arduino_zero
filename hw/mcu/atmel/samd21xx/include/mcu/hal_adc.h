/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   hal_adc.h
 * Author: paulfdietrich
 *
 * Created on March 28, 2016, 2:41 PM
 */

#ifndef _SAMD21_HAL_ADC_H
#define _SAMD21_HAL_ADC_H

/* this structure is passed to the BSP for configuration options */
enum samd_adc_reference_voltage {
	SAMD21_ADC_REFERENCE_INT1V,
	SAMD21_ADC_REFERENCE_INTVCC0,
	SAMD21_ADC_REFERENCE_INTVCC1,
	SAMD21_ADC_REFERENCE_AREFA,
	SAMD21_ADC_REFERENCE_AREFB,
};

enum samd_adc_analog_channel {
    SAMD21_ANALOG_0 = 0,
    SAMD21_ANALOG_1,
    SAMD21_ANALOG_2,
    SAMD21_ANALOG_3,
    SAMD21_ANALOG_4,
    SAMD21_ANALOG_5,
    SAMD21_ANALOG_6,
    SAMD21_ANALOG_7,
    SAMD21_ANALOG_8,
    SAMD21_ANALOG_9,
    SAMD21_ANALOG_10,
    SAMD21_ANALOG_11,
    SAMD21_ANALOG_12,
    SAMD21_ANALOG_13,
    SAMD21_ANALOG_14,
    SAMD21_ANALOG_15,
    SAMD21_ANALOG_16,
    SAMD21_ANALOG_17,
    SAMD21_ANALOG_18,
    SAMD21_ANALOG_19,
    SAMD21_ANALOG_TEMP,
    SAMD21_ANALOG_BANDGAP,
    SAMD21_ANALOG_SCALEDCOREVCC,
    SAMD21_ANALOG_SCALEDIOVCC,
    SAMD21_ANALOG_ADC_DAC,  
    SAMD21_ANALOG_MAX,
};

enum samd_adc_resolution {
    SAMD21_RESOLUTION_8_BITS,
    SAMD21_RESOLUTION_10_BITS,
    SAMD21_RESOLUTION_12_BITS,
};

enum samd_adc_gain {
    SAMD21_GAIN_DIV2,
    SAMD21_GAIN_1X,
    SAMD21_GAIN_2X,
    SAMD21_GAIN_4X,
    SAMD21_GAIN_8X,
    SAMD21_GAIN_16X,
};

struct samd21_adc_config {
    enum samd_adc_reference_voltage volt;
    enum samd_adc_resolution        resolution_bits;
    enum samd_adc_gain              gain;
    int                             voltage_mvolts;
};

/* This creates a new ADC object for this ADC source */
struct hal_adc * 
samd21_adc_create(enum samd_adc_analog_channel chan, 
                  const struct samd21_adc_config *pconfig);


#endif /* _SAMD21_HAL_ADC_H */

