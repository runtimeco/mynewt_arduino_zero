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

#define WIFI_SSID_MAX           32
#define WIFI_BSSID_LEN           6
#define WIFI_SCAN_CNT_MAX       20
#define WIFI_KEY_MAX            64
#define WIFI_SSID_EMPTY(ssid)   (ssid)[0] == '\0'

struct wifi_ap {
    char wa_ssid[WIFI_SSID_MAX + 1];
    char wa_bssid[WIFI_BSSID_LEN + 1];
    int8_t wa_rssi;
    uint8_t wa_key_type;
    uint8_t wa_channel;
};

struct winc1500 {
    enum  {
        STOPPED = 0,
        INIT,
        CONNECTING,
        DHCP_WAIT,
        CONNECTED,
        SCANNING
    } w_state, w_tgt;
    struct os_callout_func w_cb;
    struct os_event w_event;
    uint8_t w_scan_cnt;
    uint8_t w_scan_idx;
    struct wifi_ap w_scan[WIFI_SCAN_CNT_MAX];
    char w_ssid[WIFI_SSID_MAX + 1];
    char w_key[WIFI_KEY_MAX + 1];
    uint8_t w_myip[4];
};

extern struct winc1500 winc1500;
extern struct shell_cmd wifi_cli_cmd;

struct wifi_ap *wifi_find_ap(struct winc1500 *w, char *ssid);

#endif
