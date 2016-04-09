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

#include <os/os.h>
#include <console/console.h>
#include <arduino_test/arduino_test.h>
#include <hal/hal_adc.h>
#include <hal/hal_pwm.h>
#include <hal/hal_gpio.h>
#include <hal/hal_dac.h>
#include <shell/shell.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

struct arduino_pin_map_entry {
    char *name;
    uint8_t   sysid;
    uint8_t   entry_id;
};

/* maps the pin names to the sys id and to an index to reference dynamic data */
static const struct arduino_pin_map_entry 
pin_map[] = {
    {"A0", ARDUINO_ZERO_A0, 0},
    {"A1", ARDUINO_ZERO_A1, 1},
    {"A2", ARDUINO_ZERO_A2, 2},
    {"A3", ARDUINO_ZERO_A3, 3},
    {"A4", ARDUINO_ZERO_A4, 4},
    {"A5", ARDUINO_ZERO_A5, 5},
    {"D2", ARDUINO_ZERO_D2, 6},
    {"D3", ARDUINO_ZERO_D3, 7},
    {"D4", ARDUINO_ZERO_D4, 8},
    {"D5", ARDUINO_ZERO_D5, 9},
    {"D6", ARDUINO_ZERO_D6, 10},
    {"D7", ARDUINO_ZERO_D7, 11},
    {"D8", ARDUINO_ZERO_D8, 12},
    {"D9", ARDUINO_ZERO_D9, 13},
    {"D10", ARDUINO_ZERO_D10, 14},
    {"D11", ARDUINO_ZERO_D11, 15},
    {"D12", ARDUINO_ZERO_D12, 16},
    {"D13", ARDUINO_ZERO_D13, 17},
};

#define ARDUINO_NUM_DEVS    (sizeof(pin_map)/sizeof(struct arduino_pin_map_entry))

/* the types of things we can configure this to */
enum interface_type {
    INTERFACE_UNINITIALIZED = 0,
    INTERFACE_GPIO_OUT,
    INTERFACE_GPIO_IN,
    INTERFACE_ADC,
    INTERFACE_DAC,
    INTERFACE_PWM_DUTY,
    INTERFACE_PWM_FREQ,
};

typedef struct {
    char    *name;
    uint8_t  entry_id;
    int      min_value;
    int      max_value;    
    char * desc; 
} interface_into_t;

const interface_into_t interface_info[] = {
    {"none",     INTERFACE_UNINITIALIZED, 0, -1,     "Unused Pin"},
    {"gpio_out", INTERFACE_GPIO_OUT,      0, 1,      "Binary Output Port"},
    {"gpio_in",  INTERFACE_GPIO_IN,       0, -1,     "Binary Input Port"},
    {"adc",      INTERFACE_ADC,           0, -1,     "Analog to Digital Input"},
    {"dac",      INTERFACE_DAC,           0, 2048,   "Digital to Analog Output"},
    {"pwm_duty", INTERFACE_PWM_DUTY,      0, 65535,  "Pulse Width Mod. by Duty Cycle" },  
    {"pwm_freq", INTERFACE_PWM_FREQ,      60, 10000, "PWM as Frequency Generator" },  
};

#define INTERFACE_CNT   (sizeof(interface_info)/sizeof(interface_into_t))

/* The dynamic data that says what each pin is doing */
typedef struct {
    int type;
    int value;
    union {
        int    gpio_pin;
        struct hal_adc *padc;
        struct hal_dac *pdac;
        struct hal_pwm *ppwm;
    };    
} interfaces_t;

/* internal state for this CPI */
static interfaces_t interface_map[ARDUINO_NUM_DEVS];

static int arduino_test_cli_cmd(int argc, char **argv);

static struct shell_cmd arduino_test_cmd_struct = {
    .sc_cmd = "arduino",
    .sc_cmd_func = arduino_test_cli_cmd
};

static int
arduino_pinstr_to_entry(char *pinstr) {
    int i;
    
    for(i = 0; i < ARDUINO_NUM_DEVS; i++) {
        if (strcmp(pinstr, pin_map[i].name) == 0) {
            return pin_map[i].entry_id;
        }
    }
    return -1;
}

static int 
arduino_devstr_to_dev(char *devstr) {
    int i;
    for(i = 0; i < INTERFACE_CNT; i++) {
        if(strcmp(devstr, interface_info[i].name) == 0) {
            return i;
        }
    }
    return -1;
}

static int
arduino_set_device(int entry_id, int devtype) {    
    int rc = -1;
    const struct arduino_pin_map_entry *pmap = &pin_map[entry_id];
    interfaces_t *pint = &interface_map[entry_id];

    assert(entry_id < ARDUINO_NUM_DEVS);
    
    if(pint->type) {
        console_printf("Device already Initialized as %s\n",
                interface_info[entry_id].name);
        return rc;
    }
    
    switch (devtype) {
        case INTERFACE_GPIO_OUT:
            rc = hal_gpio_init_out(pmap->sysid, 0);
            if(rc == 0) {
                pint->gpio_pin = pmap->sysid;
                pint->type = INTERFACE_GPIO_OUT;                
            }
            break;
        case INTERFACE_GPIO_IN:
            rc = hal_gpio_init_in(pmap->sysid, GPIO_PULL_NONE);
            if(rc == 0) {
                pint->gpio_pin = pmap->sysid;
                pint->type = INTERFACE_GPIO_IN;
            }
            break;            
        case INTERFACE_ADC:
        {
            struct hal_adc *padc;
            padc = hal_adc_init(pmap->sysid);
            if(NULL != padc) {
                rc = 0;
                pint->padc = padc;
                pint->type = INTERFACE_ADC;
            }
            break;
        }
        case INTERFACE_DAC:
        {
            console_printf("Not supported\n");
            rc = -3;
            break;   
        }
        case INTERFACE_PWM_DUTY:
        {
            struct hal_pwm *ppwm;
            ppwm = hal_pwm_init(pmap->sysid);
            if(NULL != ppwm) {
                rc = 0;
                
                if(hal_pwm_enable_duty_cycle(ppwm,0) == 0) {
                    pint->ppwm = ppwm;
                    pint->type = INTERFACE_PWM_DUTY;
                } else {
                    /* does this not support duty cycle */
                    free(ppwm);
                    rc = -2;
                }
            }            
            break;
        }       
        case INTERFACE_PWM_FREQ:
        {
            struct hal_pwm *ppwm;
            ppwm = hal_pwm_init(pmap->sysid);
            if(NULL != ppwm) {
                rc = 0;
                if(hal_pwm_set_frequency(ppwm, 200) == 0) {
                    pint->ppwm = ppwm;
                    pint->type = INTERFACE_PWM_FREQ;
                } else {
                    /* does this not support duty cycle */
                    free(ppwm);
                    rc = -2;                    
                }
            }            
            break;            
        }        
        case INTERFACE_UNINITIALIZED:
            /* HMM do we free memory here. We didnt make any way to 
             * reassign these tings */
            console_printf("Not supported\n");
            rc = -3;
            break;            
        default:
            rc = -2;
    }
    
    if (0 == rc) {
       pint->value = 0; 
    }
    return rc;
}

static int
arduino_write(int entry_id, int value) {
    int rc = -1;
    interfaces_t *pint = &interface_map[entry_id];
    
    if((value > interface_info[pint->type].max_value) ||
       (value < interface_info[pint->type].min_value)) {
        console_printf("Value %d out of range for device \n", value);
        return rc;
    }

    switch (pint->type) {
        case INTERFACE_UNINITIALIZED:
        case INTERFACE_GPIO_IN:
        case INTERFACE_ADC:
        default:
            /* these don't support write */
            break;
        case INTERFACE_GPIO_OUT:
            if(value) {
                hal_gpio_set(pint->gpio_pin);
            } else {
                hal_gpio_clear(pint->gpio_pin);                
            }
            rc = 0;
            break;
        case INTERFACE_DAC:
            return -2;
            break;
        case INTERFACE_PWM_DUTY:
            rc = hal_pwm_enable_duty_cycle(pint->ppwm, (uint16_t) value);
            break;
        case INTERFACE_PWM_FREQ:
            rc = hal_pwm_set_frequency(pint->ppwm, value);
            hal_pwm_enable_duty_cycle(pint->ppwm, 0x8000);
            break;            
    }
    
    /* store what we wrote */
    if(0 == rc) {
        pint->value = value;        
    }
    return rc;
}

static int
arduino_read(int entry_id, int *value) {
    int rc = -1;
    interfaces_t *pint = &interface_map[entry_id];
    
    switch (pint->type) {
        case INTERFACE_UNINITIALIZED:
            break;
        case INTERFACE_GPIO_OUT:
        case INTERFACE_PWM_DUTY:
        case INTERFACE_DAC:
        case INTERFACE_PWM_FREQ:            
            *value = pint->value;            
            rc = 0;
            break;            
        case INTERFACE_GPIO_IN:
            *value = hal_gpio_read(pint->gpio_pin);
            rc = 0;
            break;
        case INTERFACE_ADC:
            *value = hal_adc_read(pint->padc);            
            if(*value >= 0) {
                rc = 0;
            }
    }
    return rc;
}

static void 
arduino_show(int entry_id) {
    int i;
    int min = 0;
    int max = ARDUINO_NUM_DEVS;
    char buf[32];
    
    if(entry_id >= 0) {
        min = entry_id;
        max = entry_id + 1;
    }
    
    console_printf("        %5s%9s%10s\n", "Pin", "Function", "Value");
    for( i = min; i < max; i++) {
        int value;
        interfaces_t *pint = &interface_map[i];
        const interface_into_t *pinfo;
        
        pinfo = &interface_info[pint->type];
        
        if(arduino_read(i, &value) != 0) {
            sprintf(buf, "N/A");
        } else {
            sprintf(buf, "%d", value);            
        }
        console_printf("        %5s%9s%10s\n",
                        pin_map[i].name, pinfo->name, buf);
    }    
}

static void
usage(void) {
    int i;
    char buf[256];
    char *ptr;
    
    console_printf("cmd: arduino <set|write|read|show> <args>\n");
    console_printf("cmd:   set <pin> <function>\n");
    console_printf("          Sets a pin to a desired function.  Not \n");
    console_printf("          all pins support all functions. This \n");
    console_printf("          will return an error if the function is \n");
    console_printf("          not supported or if the pin is already \n");
    console_printf("          set to a function.\n");
    console_printf("cmd:   read <pin>\n");
    console_printf("          Reads the value from a pin. If the function is \n");
    console_printf("          a read function, returns the value read.  If the \n");
    console_printf("          function is a write function, reads the previously \n");
    console_printf("          written value \n");
    console_printf("cmd:   write <pin> <value>\n");
    console_printf("          Write a value to a pin.  If the pin is set to a\n");
    console_printf("          read only function, an error is returned. The \n");
    console_printf("          legal value depends on the function of the pin\n");
    console_printf("cmd:   show {pin}\n");
    console_printf("          With argument pin, shows information about that\n");
    console_printf("          specific pin. Otherwise, shows information about\n");
    console_printf("          all pins \n");
    console_printf("\n");        
    console_printf("       Valid Pins\n");
    ptr = buf;
    ptr += sprintf(buf, "          ");
    for(i = 0; i < ARDUINO_NUM_DEVS; i++) {
        ptr += sprintf(ptr, "%s ", pin_map[i].name);
        if(i && ((i & 15) == 0)) {
            console_printf("%s\n", buf);
            ptr = buf;
            ptr += sprintf(buf, "          ");            
        }
    }
    if(~i || ((i & 15) != 0)) {
        console_printf("%s\n", buf);
    }
    
    console_printf("\n");
    console_printf("       Valid Functions\n");
    console_printf("          %9s%8s%8s %s\n",
                    "name", "min_val", "max_val", "Description");
    for(i = 0; i < INTERFACE_CNT; i++) {
        console_printf( "          %9s%8d%8d %s\n",
                interface_info[i].name,
                interface_info[i].min_value,
                interface_info[i].max_value,
                interface_info[i].desc);
    }
}

static int
arduino_test_cli_cmd(int argc, char **argv)
{
    int rc;
    if (argc == 1) {
        usage();
        return 0;
    }

    if (!strcmp(argv[1], "set")) {
        int entry;
        int dev;
        
        if(argc != 4) {
            usage();
            return 0;
        }
        
        entry = arduino_pinstr_to_entry(argv[2]);
        
        if(entry < 0) {
            console_printf("Invalid pin %s \n", argv[2]);
            usage();
            return -1;                                
        }

        dev = arduino_devstr_to_dev(argv[3]);
        
        if(dev < 0) {
            console_printf("Invalid Device %s\n", argv[3]);
            usage();
            return -1;                
        }
        
        rc = arduino_set_device(entry, dev);
        if(rc) {
            console_printf("Unable to set pin %s to %s, err=%d\n", argv[2], argv[3], rc);
        } else {
            console_printf("Set pin %s to %s\n", argv[2], argv[3]);            
        }  
    } else if (!strcmp(argv[1], "write")) { 
        int entry;
        int value;

        if(argc != 4) {
            usage();
            return 0;
        }
        
        entry = arduino_pinstr_to_entry(argv[2]);
        
        if(entry < 0) {
            console_printf("Invalid pin %s \n", argv[2]);
            usage();
            return -1;                                
        }

        value = atoi(argv[3]);

        rc = arduino_write(entry, value);
        if(rc) {
            console_printf("Unable to write %s to %d, err=%d\n", argv[2], value, rc);
        } else {
            console_printf("Write pin %s to %d\n", argv[2], value);            
        }               
    }  else if (!strcmp(argv[1], "read")) { 
        int entry;
        int value;

        entry = arduino_pinstr_to_entry(argv[2]);
        
        if(argc != 3) {
            usage();
            return 0;
        }
        
        if(entry < 0) {
            console_printf("Invalid pin %s \n", argv[2]);
            usage();
            return -1;                                
        }

        rc = arduino_read(entry, &value);
        if(rc) {
            console_printf("Unable to read %s, err=%d\n", argv[2], rc);
        } else {
            console_printf("Read pin %s value %d\n", argv[2], value);            
        }              
    } else if (!strcmp(argv[1], "show")) {
        int entry_id = -1;
        if(argc == 3) {
            entry_id = arduino_pinstr_to_entry(argv[2]);

            if(entry_id < 0) {
                console_printf("Invalid pin %s \n", argv[2]);
                usage();
                return -1;                                
            }  
        } else if (argc != 2 ) {
            usage();
            return -1;             
        }                  
        arduino_show(entry_id);
    }else if ( !strcmp(argv[1], "?") || !strcmp(argv[1], "help")) {
        usage();
    }
    return 0;
}

int
arduino_test_init(void) {
    shell_cmd_register(&arduino_test_cmd_struct);
    return 0;
}
