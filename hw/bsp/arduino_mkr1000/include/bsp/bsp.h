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
#include <mcu/mcu.h>

#include <syscfg/syscfg.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t _ram_start;
#define RAM_SIZE        0x00008000

#define LED_BLINK_PIN   (20)
#define LED_2           (21)
#define BSP_WINC1500_SPI_PORT   2
#define CONSOLE_UART_SPEED      115200

/*
 * Wiring of WINC1500 chip to SAMD21.
 */
#define WINC1500_PIN_RESET		MCU_GPIO_PORTA(27)	/* PA27 */
#define WINC1500_PIN_WAKE		MCU_GPIO_PORTB(8)	/* PB08 */
#define WINC1500_PIN_IRQ		MCU_GPIO_PORTB(9)	/* PB09 */
#define WINC1500_PIN_ENABLE		MCU_GPIO_PORTA(28)	/* PA28 */

#define WINC1500_SPI_SPEED		4000000
#define WINC1500_SPI_SSN		MCU_GPIO_PORTA(14)	/* PA14 */
#define WINC1500_SPI_SCK		MCU_GPIO_PORTA(13)	/* PA13 */
#define WINC1500_SPI_MOSI		MCU_GPIO_PORTA(12)	/* PA12 */
#define WINC1500_SPI_MISO		MCU_GPIO_PORTA(15)	/* PA15 */

#ifdef __cplusplus
}
#endif

#endif  /* __MKR1000_BSP_H__ */
