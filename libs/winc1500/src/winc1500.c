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
#include <assert.h>

#include <os/os.h>
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include <shell/shell.h>
#include <console/console.h>

#include "winc1500/driver/m2m_wifi.h"

#include "winc1500_priv.h"

#define WINC1500_POLL_ITVL	100
#define WINC1500_EV_STATE	OS_EVENT_T_PERUSER

static struct os_task winc1500_os_task;
static struct os_eventq winc1500_evq;
struct winc1500 winc1500;

static void
winc1500_tgt_state(struct winc1500 *w, int state)
{
    w->w_tgt = state;
    os_eventq_put(&winc1500_evq, &w->w_event);
}

static void
winc1500_scan_done(struct winc1500 *w, int status)
{
    struct wifi_ap *ap = NULL;

    console_printf("scan_results: %d\n", status);
    if (status) {
        winc1500_tgt_state(w, STOPPED);
        return;
    }

    /*
     * XXX decide what to do with scan results here.
     */
    if (!WIFI_SSID_EMPTY(w->w_ssid)) {
        ap = wifi_find_ap(w, w->w_ssid);
    }
    if (ap) {
        winc1500_tgt_state(w, CONNECTING);
    } else {
        winc1500_tgt_state(w, INIT);
    }
}

static void
winc1500_connect_done(struct winc1500 *w, int status)
{
    console_printf("connect_done : %d\n", status);
    if (status) {
        winc1500_tgt_state(w, INIT);
        return;
    }
    winc1500_tgt_state(w, DHCP_WAIT);
}

static void
winc1500_dhcp_done(struct winc1500 *w, uint8_t *ip)
{
    console_printf("dhcp done %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    winc1500_tgt_state(w, CONNECTED);
}

static void
winc1500_disconnect_done(struct winc1500 *w, int status)
{
    console_printf("disconnect : %d\n", status);
    winc1500_tgt_state(w, INIT);
}

/*
 * Called within winc1500 task context to report incoming events.
 */
static void
wifi_callback(uint8_t msg_type, void *msg_data)
{
    struct winc1500 *w;
    tstrM2mScanDone *scan_done;
    tstrM2mWifiscanResult *scan;
    tstrM2mWifiStateChanged *state;
    tstrSystemTime *time;
    struct wifi_ap *wap;
    int rc;

    w = &winc1500;
    switch (msg_type) {
    case M2M_WIFI_RESP_SCAN_DONE:
        if (w->w_state != SCANNING) {
            break;
        }
        scan_done = (tstrM2mScanDone *)msg_data;
        console_printf("scan_done: %d\n", scan_done->u8NumofCh);
        w->w_scan_cnt = scan_done->u8NumofCh;
        if (w->w_scan_cnt > WIFI_SCAN_CNT_MAX) {
            w->w_scan_cnt = WIFI_SCAN_CNT_MAX;
        }
        if (w->w_scan_cnt > 0) {
            w->w_scan_idx = 0;
            rc = m2m_wifi_req_scan_result(0);
            if (rc) {
                winc1500_scan_done(w, -1);
            }
        }
        break;
    case M2M_WIFI_RESP_SCAN_RESULT:
        if (w->w_state != SCANNING) {
            break;
        }
        scan = (tstrM2mWifiscanResult *)msg_data;
        wap = (struct wifi_ap *)&w->w_scan[w->w_scan_idx];
        memcpy(wap->wa_ssid, scan->au8SSID, sizeof(wap->wa_ssid));
        memcpy(wap->wa_bssid, scan->au8BSSID, sizeof(wap->wa_bssid));
        wap->wa_rssi = scan->s8rssi;
        wap->wa_key_type = scan->u8AuthType;
        wap->wa_channel = scan->u8ch;
        ++w->w_scan_idx;
        if (w->w_scan_idx < w->w_scan_cnt) {
            rc = m2m_wifi_req_scan_result(w->w_scan_idx);
            if (rc) {
                winc1500_scan_done(w, -1);
            }
        } else {
            winc1500_scan_done(w, 0);
        }
        break;
    case M2M_WIFI_RESP_CON_STATE_CHANGED:
        state = (tstrM2mWifiStateChanged *)msg_data;
        if (state->u8CurrState == M2M_WIFI_CONNECTED) {
            /* connected */
            if (w->w_state != CONNECTING) {
                break;
            }
            winc1500_connect_done(w, 0);
        } else if (state->u8CurrState == M2M_WIFI_DISCONNECTED) {
            /* disconnected */
            if (w->w_state == CONNECTING) {
                winc1500_connect_done(w, state->u8ErrCode);
            } else {
                winc1500_disconnect_done(w, state->u8ErrCode);
            }
        }
        break;
    case M2M_WIFI_REQ_DHCP_CONF:
        winc1500_dhcp_done(w, msg_data);
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

struct wifi_ap *
wifi_find_ap(struct winc1500 *w, char *ssid)
{
    int i;

    for (i = 0; i < w->w_scan_cnt; i++) {
        if (!strcmp(w->w_scan[i].wa_ssid, ssid)) {
            return &w->w_scan[i];
        }
    }
    return NULL;
}

static void
winc1500_events(void *arg)
{
    struct winc1500 *w = (struct winc1500 *)arg;

    m2m_wifi_handle_events(NULL);
    os_callout_reset(&w->w_cb.cf_c, WINC1500_POLL_ITVL);
}

int
winc1500_start(struct winc1500 *w)
{
    if (w->w_state != STOPPED) {
        return -1;
    }
    winc1500_tgt_state(w, INIT);
    return 0;
}

int
winc1500_stop(struct winc1500 *w)
{
    winc1500_tgt_state(w, STOPPED);
    return 0;
}

int
winc1500_connect(struct winc1500 *w)
{
    switch (w->w_state) {
    case STOPPED:
        return -1;
    case INIT:
        if (WIFI_SSID_EMPTY(w->w_ssid)) {
            return -1;
        }
        winc1500_tgt_state(w, CONNECTING);
        return 0;
    default:
        return -1;
    }
}

int
winc1500_scan_start(struct winc1500 *w)
{
    if (w->w_state == INIT) {
        winc1500_tgt_state(w, SCANNING);
        return 0;
    }
    return -1;
}

static void
winc1500_step(struct winc1500 *w)
{
    tstrWifiInitParam init_param;
    int rc;
    struct wifi_ap *ap;

    switch (w->w_tgt) {
    case STOPPED:
        if (w->w_state != STOPPED) {
            m2m_wifi_deinit(NULL);
            w->w_state = STOPPED;
        }
        break;
    case INIT:
        if (w->w_state == STOPPED) {
            init_param.pfAppWifiCb = wifi_callback;
            rc = m2m_wifi_init(&init_param);
            console_printf("wifi_init : %d\n", rc);
            if (!rc) {
                w->w_state = INIT;
                os_callout_reset(&w->w_cb.cf_c, WINC1500_POLL_ITVL);
            }
        } else if (w->w_state == SCANNING) {
            w->w_state = w->w_tgt;
        }
        break;
    case SCANNING:
        if (w->w_state == INIT) {
            rc = m2m_wifi_set_scan_region(NORTH_AMERICA);
            if (rc != 0) {
                break;
            }
            rc = m2m_wifi_request_scan(M2M_WIFI_CH_ALL);
            console_printf("wifi_request_scan : %d\n", rc);
            if (rc != 0) {
                break;
            }
            w->w_state = SCANNING;
        } else {
            w->w_tgt = w->w_state;
        }
        break;
    case CONNECTING:
        if (w->w_state == INIT || w->w_state == SCANNING) {
            ap = wifi_find_ap(w, w->w_ssid);
            if (!ap) {
                winc1500_tgt_state(w, SCANNING);
                break;
            }
            rc = m2m_wifi_connect(w->w_ssid, strlen(w->w_ssid),
              ap->wa_key_type, w->w_key, M2M_WIFI_CH_ALL);
            console_printf("wifi_connect : %d\n", rc);
            if (rc == 0) {
                w->w_state = CONNECTING;
            } else {
                w->w_tgt = STOPPED;
            }
        }
        break;
    case DHCP_WAIT:
        w->w_state = w->w_tgt;
        break;
    case CONNECTED:
        w->w_state = w->w_tgt;
        break;
    default:
        console_printf("winc1500_step() unknown tgt : %d\n", w->w_tgt);
        w->w_state = w->w_tgt;
        break;
    }
}

static void
winc1500_task(void *arg)
{
    struct os_event *ev;
    struct os_callout_func *cf;
    struct winc1500 *w;

    while ((ev = os_eventq_get(&winc1500_evq))) {
        switch (ev->ev_type) {
        case OS_EVENT_T_TIMER:
            cf = (struct os_callout_func *)ev;
            cf->cf_func(CF_ARG(cf));
            break;
        case WINC1500_EV_STATE:
            w = (struct winc1500 *)ev->ev_arg;
            while (w->w_state != w->w_tgt) {
                winc1500_step(w);
            }
            break;
        default:
            break;
        }
    }
}

int
winc1500_task_init(uint8_t prio, os_stack_t *stack, uint16_t stack_size)
{
    int rc;
    struct winc1500 *w = &winc1500;

#ifdef SHELL_PRESENT
    shell_cmd_register(&wifi_cli_cmd);
#endif

    os_eventq_init(&winc1500_evq);
    os_callout_func_init(&w->w_cb, &winc1500_evq, winc1500_events, w);
    w->w_event.ev_type = WINC1500_EV_STATE;
    w->w_event.ev_arg = w;

    rc = hal_gpio_init_out(WINC1500_PIN_RESET, 0); /* reset when 0 */
    assert(rc == 0);
    rc = hal_gpio_init_out(WINC1500_PIN_ENABLE, 0); /* disabled when 0 */
    assert(rc == 0);
    rc = hal_gpio_init_out(WINC1500_PIN_WAKE, 0);
    assert(rc == 0);

    nm_bsp_reset();

    return os_task_init(&winc1500_os_task, "winc1500", winc1500_task, NULL,
      prio, OS_WAIT_FOREVER, stack, stack_size);
}
