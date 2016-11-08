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
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

#include <os/os.h>
#include <os/endian.h>
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include <shell/shell.h>
#include <console/console.h>

#include <wifi_mgmt/wifi_mgmt.h>
#include <wifi_mgmt/wifi_mgmt_if.h>

#include "winc1500/driver/m2m_wifi.h"

#include "winc1500_priv.h"

#define WINC1500_POLL_ITVL      100

struct winc1500 winc1500;

static int winc1500_start(struct wifi_if *);
static void winc1500_stop(struct wifi_if *);
static int winc1500_scan_start(struct wifi_if *);
static int winc1500_connect(struct wifi_if *, struct wifi_ap *);
static void winc1500_disconnect(struct wifi_if *);

static const struct wifi_if_ops winc1500_ops = {
    .wio_init = winc1500_start,
    .wio_deinit = winc1500_stop,
    .wio_scan_start = winc1500_scan_start,
    .wio_connect = winc1500_connect,
    .wio_disconnect = winc1500_disconnect
};

/*
 * Called within winc1500 task context to report incoming events.
 */
static void
winc1500_callback(uint8_t msg_type, void *msg_data)
{
    struct winc1500 *w;
    struct wifi_if *wi;
    tstrM2mScanDone *scan_done;
    tstrM2mWifiscanResult *scan;
    tstrM2mWifiStateChanged *state;
    tstrM2MIPConfig *dhcp;
    tstrSystemTime *time;
    struct wifi_ap ap;
    int rc;

    w = &winc1500;
    wi = &w->w_if;
    switch (msg_type) {
    case M2M_WIFI_RESP_SCAN_DONE:
        if (wi->wi_state != SCANNING) {
            break;
        }
        scan_done = (tstrM2mScanDone *)msg_data;
        w->w_scan_cnt = scan_done->u8NumofCh;
        if (w->w_scan_cnt > 0) {
            w->w_scan_idx = 0;
            rc = m2m_wifi_req_scan_result(0);
            if (rc) {
                wifi_scan_done(wi, -1);
            }
        }
        break;
    case M2M_WIFI_RESP_SCAN_RESULT:
        if (wi->wi_state != SCANNING) {
            break;
        }
        scan = (tstrM2mWifiscanResult *)msg_data;
        memcpy(ap.wa_ssid, scan->au8SSID, sizeof(ap.wa_ssid));
        memcpy(ap.wa_bssid, scan->au8BSSID, sizeof(ap.wa_bssid));
        ap.wa_rssi = scan->s8rssi;
        ap.wa_key_type = scan->u8AuthType;
        ap.wa_channel = scan->u8ch;
        wifi_scan_result(wi, &ap);
        ++w->w_scan_idx;
        if (w->w_scan_idx < w->w_scan_cnt) {
            rc = m2m_wifi_req_scan_result(w->w_scan_idx);
            if (rc) {
                wifi_scan_done(wi, -1);
            }
        } else {
            wifi_scan_done(wi, 0);
        }
        break;
    case M2M_WIFI_RESP_CON_STATE_CHANGED:
        state = (tstrM2mWifiStateChanged *)msg_data;
        if (state->u8CurrState == M2M_WIFI_CONNECTED) {
            /* connected */
            if (wi->wi_state != CONNECTING) {
                break;
            }
            wifi_connect_done(wi, 0);
        } else if (state->u8CurrState == M2M_WIFI_DISCONNECTED) {
            /* disconnected */
            w->w_up = 0;
            if (wi->wi_state == CONNECTING) {
                wifi_connect_done(wi, state->u8ErrCode);
            } else {
                wifi_disconnected(wi, state->u8ErrCode);
            }
        }
        break;
    case M2M_WIFI_REQ_DHCP_CONF:
        dhcp = (tstrM2MIPConfig *)msg_data;
        w->w_up = 1;
        w->w_addr = dhcp->u32StaticIP;
        if (dhcp->u32SubnetMask) {
            w->w_plen = 33 - ffs(ntohl(dhcp->u32SubnetMask));
        } else {
            w->w_plen = 0;
        }
        wifi_dhcp_done(&w->w_if, (uint8_t *)&dhcp->u32StaticIP);
        break;
    case M2M_WIFI_RESP_GET_SYS_TIME:
        time = (tstrSystemTime *)msg_data;
        console_printf("get sys time response %d.%d.%d-%d.%d.%d\n",
          time->u16Year, time->u8Month, time->u8Day,
          time->u8Hour, time->u8Minute, time->u8Second);
        break;
    default:
        console_printf("wifi_callback(%d, %x)\n", msg_type, (unsigned)msg_data);
        break;
    }
}

static void
winc1500_events(struct os_event *ev)
{
    struct winc1500 *w = (struct winc1500 *)ev->ev_arg;

    os_mutex_pend(&w->w_if.wi_mtx, OS_TIMEOUT_NEVER);
    m2m_wifi_handle_events(NULL);
    os_mutex_release(&w->w_if.wi_mtx);
    winc1500_socket_poll();
    os_callout_reset(&w->w_timer, WINC1500_POLL_ITVL);
}

static int
winc1500_start(struct wifi_if *wi)
{
    struct winc1500 *w = (struct winc1500 *)wi;
    tstrWifiInitParam init_param;
    int rc;

    init_param.pfAppWifiCb = winc1500_callback;
    rc = m2m_wifi_init(&init_param);
    if (rc == 0) {
        os_callout_reset(&w->w_timer, WINC1500_POLL_ITVL);
    }
    winc1500_socket_start();
    return rc;
}

static void
winc1500_stop(struct wifi_if *wi)
{
    struct winc1500 *w = (struct winc1500 *)wi;
    w->w_up = 0;

    m2m_wifi_deinit(NULL);
}

static int
winc1500_scan_start(struct wifi_if *wi)
{
    int rc;

    rc = m2m_wifi_set_scan_region(NORTH_AMERICA);
    if (rc) {
        return rc;
    }
    return m2m_wifi_request_scan(M2M_WIFI_CH_ALL);
}

static int
winc1500_connect(struct wifi_if *wi, struct wifi_ap *ap)
{
    return m2m_wifi_connect(wi->wi_ssid, strlen(wi->wi_ssid),
              ap->wa_key_type, wi->wi_key, M2M_WIFI_CH_ALL);
}

static void
winc1500_disconnect(struct wifi_if *wi)
{
    struct winc1500 *w = (struct winc1500 *)wi;
    w->w_up = 0;
    m2m_wifi_disconnect();
}

int
winc1500_init(void)
{
    int rc;
    struct winc1500 *w = &winc1500;

    rc = wifi_if_register(&w->w_if, &winc1500_ops);
    if (rc) {
        return -1;
    }
    os_callout_init(&w->w_timer, &wifi_evq, winc1500_events, w);

    rc = hal_gpio_init_out(WINC1500_PIN_RESET, 0); /* reset when 0 */
    assert(rc == 0);
#ifdef WINC1500_PIN_ENABLE
    rc = hal_gpio_init_out(WINC1500_PIN_ENABLE, 0); /* disabled when 0 */
    assert(rc == 0);
#endif
#ifdef WINC1500_PIN_WAKE
    rc = hal_gpio_init_out(WINC1500_PIN_WAKE, 0);
    assert(rc == 0);
#endif

    winc1500_socket_init();

    return 0;
}
