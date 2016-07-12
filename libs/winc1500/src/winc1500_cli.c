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
#ifdef SHELL_PRESENT
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include <shell/shell.h>
#include <console/console.h>

#include "winc1500/winc1500.h"
#include "winc1500/driver/m2m_types.h"
#include "winc1500_priv.h"

static int
wifi_cli(int argc, char **argv)
{
    struct winc1500 *w = &winc1500;
    int i;

    if (argc < 1) {
        return 0;
    }
    if (!strcmp(argv[1], "start")) {
        winc1500_start(w);
#if 0
    } else if (!strcmp(argv[1], "scan")) {
        rc = m2m_wifi_set_scan_region(NORTH_AMERICA);
        console_printf("wifi_set_scan_region() : %d\n", rc);
        rc = m2m_wifi_request_scan(M2M_WIFI_CH_ALL);
        console_printf("wifi_request_scan() : %d\n", rc);
    } else if (!strcmp(argv[1], "events")) {
        rc = m2m_wifi_handle_events(NULL);
        console_printf("wifi_handle_events() : %d\n", rc);
#endif
    } else if (!strcmp(argv[1], "stop")) {
        winc1500_stop(w);
    } else if (!strcmp(argv[1], "scan")) {
        winc1500_scan_start(w);
    } else if (!strcmp(argv[1], "aps")) {
        int i;
        struct wifi_ap *ap;

        console_printf("   %32s %4s %4s %s\n", "SSID", "RSSI", "chan", "sec");
        for (i = 0; i < w->w_scan_cnt; i++) {
            ap = (struct wifi_ap *)&w->w_scan[i];
            console_printf("%2d:%32s %4d %4d %s\n",
              i, ap->wa_ssid, ap->wa_rssi, ap->wa_channel,
              ap->wa_key_type ? "X" : "");
        }
    } else if (!strcmp(argv[1], "connect")) {
        if (argc < 2) {
            goto conn_usage;
        }
        i = strlen(argv[2]);
        if (i >= sizeof(w->w_ssid)) {
            goto conn_usage;
        }
        if (argc > 2) {
            i = strlen(argv[2]);
            if (i >= sizeof(w->w_key)) {
                goto conn_usage;
            }
            strcpy(w->w_key, argv[3]);
        }
        strcpy(w->w_ssid, argv[2]);
        if (winc1500_connect(w)) {
conn_usage:
            console_printf("%s %s [<ssid> [<key>]]\n",
              argv[0], argv[1]);
        }
    }
    return 0;
}

struct shell_cmd wifi_cli_cmd = {
    .sc_cmd = "wifi",
    .sc_cmd_func = wifi_cli
};

#endif
