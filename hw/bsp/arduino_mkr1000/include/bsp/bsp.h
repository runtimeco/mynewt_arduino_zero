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
#ifndef __MKR1000_BSP_H__
#define __MKR1000_BSP_H__

#include <inttypes.h>

#ifndef BSP_SYSID_H
#include <bsp/bsp_sysid.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t _ram_start;
#define RAM_SIZE        0x00008000

#define LED_BLINK_PIN   (20)
#define CONSOLE_UART    (0)

#ifdef BOOT_SERIAL
#define BOOT_SERIAL_DETECT_PIN 		43
#define BOOT_SERIAL_DETECT_PIN_CFG 	GPIO_PULL_UP
#define BOOT_SERIAL_DETECT_PIN_VAL      0
#endif

/*
 * Wiring of WINC1500 chip to SAMD21.
 */
#define WINC1500_PIN_RESET  /* PA27 */ 27
#define WINC1500_PIN_WAKE   /* PB08 */ (8 + 32)
#define WINC1500_PIN_IRQ    /* PB09 */ (9 + 32)
#define WINC1500_PIN_ENABLE /* PA28 */ 28

#define WINC1500_SPI_SPEED  4000000
#define WINC1500_SPI_SSN    /* PA14 */ 14
#define WINC1500_SPI_SCK    /* PA13 */ 13
#define WINC1500_SPI_MOSI   /* PA12 */ 12
#define WINC1500_SPI_MISO   /* PA15 */ 15

/* This defines the maximum NFFS areas (block) are in the BSPs NFS file
 * system space.  This in conjunction with flash map determines how
 * many NFS blocks there will be.  A minimum is the number of individually
 * erasable sectors in the flash area and the maximum is this number. If
 * your max is less than the number of sectors then the NFFS will combine
 * multiple sectors into an NFFS area */
#define NFFS_AREA_MAX    (8)

#ifdef __cplusplus
}
#endif

#endif  /* __MKR1000_BSP_H__ */
