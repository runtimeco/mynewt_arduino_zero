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
#ifndef BSP_SYSID_H
#define BSP_SYSID_H

#ifdef __cplusplus
extern "C" {
#endif

enum system_device_id  
{
        ARDUINO_ZERO_A0 = 0,
        ARDUINO_ZERO_A1,
        ARDUINO_ZERO_A2,
        ARDUINO_ZERO_A3,
        ARDUINO_ZERO_A4,
        ARDUINO_ZERO_A5,  
        ARDUINO_ZERO_D0,
        ARDUINO_ZERO_D1,
        ARDUINO_ZERO_D2,
        ARDUINO_ZERO_D3,
        ARDUINO_ZERO_D4,
        ARDUINO_ZERO_D5,
        ARDUINO_ZERO_D6,
        ARDUINO_ZERO_D7,
        ARDUINO_ZERO_D8,
        ARDUINO_ZERO_D9,
        ARDUINO_ZERO_D10,
        ARDUINO_ZERO_D11,
        ARDUINO_ZERO_D12,
        ARDUINO_ZERO_D13,
        /* NOTE, as we get other drivers on the new HAL, the will go here */
};

#ifdef __cplusplus
}
#endif

#endif /* BSP_SYSID_H */

