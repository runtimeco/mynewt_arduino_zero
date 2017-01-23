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
#include "syscfg/syscfg.h"
#include <sysinit/sysinit.h>
#include <os/os.h>
#include <bsp/bsp.h>
#include <console/console.h>
#include <shell/shell.h>
#include <config/config.h>
#include <hal/hal_system.h>
#include <assert.h>
#include <string.h>

#include <wifi_mgmt/wifi_mgmt.h>
#include <winc1500/winc1500.h>
#include <mn_socket/mn_socket.h>
#include <os/endian.h>

#include <inet_def_service/inet_def_service.h>

#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif

#define WIFI_TASK_PRIO          4
#define WIFI_STACK_SZ           512
static os_stack_t wifi_stack[WIFI_STACK_SZ];

static int net_cli(int argc, char **argv);
struct shell_cmd net_test_cmd = {
    .sc_cmd = "net",
    .sc_cmd_func = net_cli
};

static struct mn_socket *net_test_socket;
static struct mn_socket *net_test_socket2;

static void net_test_readable(void *arg, int err)
{
    console_printf("net_test_readable %x - %d\n", (int)arg, err);
}

static void net_test_writable(void *arg, int err)
{
    console_printf("net_test_writable %x - %d\n", (int)arg, err);
}

static const union mn_socket_cb net_test_cbs = {
    .socket.readable = net_test_readable,
    .socket.writable = net_test_writable
};

static int net_test_newconn(void *arg, struct mn_socket *new)
{
    console_printf("net_test_newconn %x - %x\n", (int)arg, (int)new);
    mn_socket_set_cbs(new, NULL, &net_test_cbs);
    net_test_socket2 = new;
    return 0;
}

static const union mn_socket_cb net_listen_cbs =  {
    .listen.newconn = net_test_newconn,
};

static int
net_cli(int argc, char **argv)
{
    int rc;
    struct mn_sockaddr_in sin;
    struct mn_sockaddr_in *sinp;
    uint16_t port;
    uint32_t addr;
    char *eptr;
    struct os_mbuf *m;

    if (argc < 2) {
        return 0;
    }
    if (!strcmp(argv[1], "udp")) {
        rc = mn_socket(&net_test_socket, MN_PF_INET, MN_SOCK_DGRAM, 0);
        console_printf("mn_socket(UDP) = %d %x\n", rc,
          (int)net_test_socket);
    } else if (!strcmp(argv[1], "tcp")) {
        rc = mn_socket(&net_test_socket, MN_PF_INET, MN_SOCK_STREAM, 0);
        console_printf("mn_socket(TCP) = %d %x\n", rc,
          (int)net_test_socket);
    } else if (!strcmp(argv[1], "connect") || !strcmp(argv[1], "bind")) {
        if (argc < 4) {
            return 0;
        }

        if (mn_inet_pton(MN_AF_INET, argv[2], &addr) != 1) {
            console_printf("Invalid address %s\n", argv[2]);
            return 0;
        }

        port = strtoul(argv[3], &eptr, 0);
        if (*eptr != '\0') {
            console_printf("Invalid port %s\n", argv[3]);
            return 0;
        }
        uint8_t *ip = (uint8_t *)&addr;

        console_printf("%d.%d.%d.%d/%d\n", ip[0], ip[1], ip[2], ip[3], port);
        memset(&sin, 0, sizeof(sin));
        sin.msin_len = sizeof(sin);
        sin.msin_family = MN_AF_INET;
        sin.msin_port = htons(port);
        sin.msin_addr.s_addr = addr;

        if (!strcmp(argv[1], "connect")) {
            mn_socket_set_cbs(net_test_socket, NULL, &net_test_cbs);
            rc = mn_connect(net_test_socket, (struct mn_sockaddr *)&sin);
            console_printf("mn_connect() = %d\n", rc);
        } else {
            mn_socket_set_cbs(net_test_socket, NULL, &net_test_cbs);
            rc = mn_bind(net_test_socket, (struct mn_sockaddr *)&sin);
            console_printf("mn_bind() = %d\n", rc);
        }
    } else if (!strcmp(argv[1], "listen")) {
            mn_socket_set_cbs(net_test_socket, NULL, &net_listen_cbs);
        rc = mn_listen(net_test_socket, 2);
        console_printf("mn_listen() = %d\n", rc);
    } else if (!strcmp(argv[1], "close")) {
        rc = mn_close(net_test_socket);
        console_printf("mn_close() = %d\n", rc);
        net_test_socket = NULL;
        if (net_test_socket2) {
            rc = mn_close(net_test_socket2);
            console_printf("mn_close() = %d\n", rc);
            net_test_socket2 = NULL;
        }
    } else if (!strcmp(argv[1], "send")) {
        if (argc < 3) {
            return 0;
        }
        m = os_msys_get_pkthdr(16, 0);
        if (!m) {
            console_printf("out of mbufs\n");
            return 0;
        }
        rc = os_mbuf_copyinto(m, 0, argv[2], strlen(argv[2]));
        if (rc < 0) {
            console_printf("can't copy data\n");
            os_mbuf_free_chain(m);
            return 0;
        }
        if (argc > 4) {
            if (mn_inet_pton(MN_AF_INET, argv[3], &addr) != 1) {
                console_printf("Invalid address %s\n", argv[2]);
                return 0;
            }

            port = strtoul(argv[4], &eptr, 0);
            if (*eptr != '\0') {
                console_printf("Invalid port %s\n", argv[3]);
                return 0;
            }
            uint8_t *ip = (uint8_t *)&addr;

            console_printf("%d.%d.%d.%d/%d\n", ip[0], ip[1], ip[2], ip[3],
              port);
            memset(&sin, 0, sizeof(sin));
            sin.msin_len = sizeof(sin);
            sin.msin_family = MN_AF_INET;
            sin.msin_port = htons(port);
            sin.msin_addr.s_addr = addr;
            sinp = &sin;
        } else {
            sinp = NULL;
        }

        if (net_test_socket2) {
            rc = mn_sendto(net_test_socket2, m, (struct mn_sockaddr *)sinp);
        } else {
            rc = mn_sendto(net_test_socket, m, (struct mn_sockaddr *)sinp);
        }
        console_printf("mn_sendto() = %d\n", rc);
    } else if (!strcmp(argv[1], "peer")) {
        if (net_test_socket2) {
            rc = mn_getpeername(net_test_socket2, (struct mn_sockaddr *)&sin);
        } else {
            rc = mn_getpeername(net_test_socket, (struct mn_sockaddr *)&sin);
        }
        console_printf("mn_getpeername() = %d\n", rc);
        uint8_t *ip = (uint8_t *)&sin.msin_addr;

        console_printf("%d.%d.%d.%d/%d\n", ip[0], ip[1], ip[2], ip[3],
          ntohs(sin.msin_port));
    } else if (!strcmp(argv[1], "recv")) {
        if (net_test_socket2) {
            rc = mn_recvfrom(net_test_socket2, &m, (struct mn_sockaddr *)&sin);
        } else {
            rc = mn_recvfrom(net_test_socket, &m, (struct mn_sockaddr *)&sin);
        }
        console_printf("mn_recvfrom() = %d\n", rc);
        if (m) {
            uint8_t *ip = (uint8_t *)&sin.msin_addr;

            console_printf("%d.%d.%d.%d/%d\n", ip[0], ip[1], ip[2], ip[3],
              ntohs(sin.msin_port));
            m->om_data[m->om_len] = '\0';
            console_printf("received %d bytes >%s<\n",
              OS_MBUF_PKTHDR(m)->omp_len, (char *)m->om_data);
            os_mbuf_free_chain(m);
        }
    } else if (!strcmp(argv[1], "mcast_join") ||
      !strcmp(argv[1], "mcast_leave")) {
        struct mn_mreq mm;
        int val;

        if (argc < 4) {
            return 0;
        }

        val = strtoul(argv[2], &eptr, 0);
        if (*eptr != '\0') {
            console_printf("Invalid itf_idx %s\n", argv[2]);
            return 0;
        }

        memset(&mm, 0, sizeof(mm));
        mm.mm_idx = val;
        mm.mm_family = MN_AF_INET;
        if (mn_inet_pton(MN_AF_INET, argv[3], &mm.mm_addr) != 1) {
            console_printf("Invalid address %s\n", argv[2]);
            return 0;
        }
        if (!strcmp(argv[1], "mcast_join")) {
            val = MN_MCAST_JOIN_GROUP;
        } else {
            val = MN_MCAST_LEAVE_GROUP;
        }
        rc = mn_setsockopt(net_test_socket, MN_SO_LEVEL, val, &mm);
        console_printf("mn_setsockopt() = %d\n", rc);
    } else if (!strcmp(argv[1], "listif")) {
        struct mn_itf itf;
        struct mn_itf_addr itf_addr;
        char addr_str[48];

        memset(&itf, 0, sizeof(itf));
        while (1) {
            rc = mn_itf_getnext(&itf);
            if (rc) {
                break;
            }
            console_printf("%d: %x %s\n", itf.mif_idx, itf.mif_flags,
              itf.mif_name);
            memset(&itf_addr, 0, sizeof(itf_addr));
            while (1) {
                rc = mn_itf_addr_getnext(&itf, &itf_addr);
                if (rc) {
                    break;
                }
                mn_inet_ntop(itf_addr.mifa_family, &itf_addr.mifa_addr,
                  addr_str, sizeof(addr_str));
                console_printf(" %s/%d\n", addr_str, itf_addr.mifa_plen);
            }
        }
    } else if (!strcmp(argv[1], "service")) {
        inet_def_service_init(os_eventq_dflt_get());
    } else {
        console_printf("unknown cmd\n");
    }
    return 0;
}

/**
 * main
 *
 * The main function for the project. This function initializes the os, calls
 * init_tasks to initialize tasks (and possibly other objects), then starts the
 * OS. We should not return from os start.
 *
 * @return int NOTE: this function should never return!
 */
int
main(int argc, char **argv)
{
    int rc;

#ifdef ARCH_sim
    mcu_sim_parse_args(argc, argv);
#endif

    sysinit();

    console_printf("Wifi test\n");

    conf_load();

    wifi_task_init(WIFI_TASK_PRIO, wifi_stack, WIFI_STACK_SZ);
    winc1500_init();
    shell_cmd_register(&net_test_cmd);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    /* os start should never return. If it does, this should be an error */
    assert(0);

    return rc;
}

