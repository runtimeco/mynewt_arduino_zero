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
#ifndef __WINC1500_PRIV_H__
#define __WINC1500_PRIV_H__

#define WINC1500_SOCK_RX_BLOCK  128

#include <wifi_mgmt/wifi_mgmt.h>

struct winc1500 {
    struct wifi_if w_if;
    struct os_callout w_timer;
    uint8_t w_scan_cnt;
    uint8_t w_scan_idx;
    uint8_t w_up:1;
    uint8_t w_plen:6;
    uint32_t w_addr;
};

extern struct winc1500 winc1500;

int winc1500_socket_init(void);
void winc1500_socket_start(void);
void winc1500_socket_poll(void);

#endif
