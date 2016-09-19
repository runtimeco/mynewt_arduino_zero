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
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash.h>
#include <console/console.h>
#include <shell/shell.h>
#include <config/config.h>
#include <hal/flash_map.h>
#include <hal/hal_system.h>
#ifdef NFFS_PRESENT
#include <fs/fs.h>
#include <nffs/nffs.h>
#include <config/config_file.h>
#elif FCB_PRESENT
#include <fcb/fcb.h>
#include <config/config_fcb.h>
#else
#error "Need NFFS or FCB for config storage"
#endif
#include <assert.h>
#include <string.h>
#include <json/json.h>

#include <wifi_mgmt/wifi_mgmt.h>
#include <winc1500/winc1500.h>
#include <mn_socket/mn_socket.h>
#include <os/endian.h>

#include <inet_def_service/inet_def_service.h>

#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif

#define SHELL_TASK_PRIO         3
#define SHELL_MAX_INPUT_LEN     256
#define SHELL_TASK_STACK_SIZE   OS_STACK_ALIGN(384)
static os_stack_t shell_stack[SHELL_TASK_STACK_SIZE];

#define WIFI_TASK_PRIO          4
#define WIFI_STACK_SZ           512
static os_stack_t wifi_stack[WIFI_STACK_SZ];

#define NET_SERVICE_PRIO        5
#define NET_SERVICE_STACK_SIZE  1024
static os_stack_t net_service_stack[NET_SERVICE_STACK_SIZE];

#ifdef NFFS_PRESENT
/* configuration file */
#define MY_CONFIG_DIR  "/cfg"
#define MY_CONFIG_FILE "/cfg/run"
#define MY_CONFIG_MAX_LINES     32

static struct conf_file my_conf = {
    .cf_name = MY_CONFIG_FILE,
    .cf_maxlines = MY_CONFIG_MAX_LINES
};
#elif FCB_PRESENT
struct flash_area conf_fcb_area[NFFS_AREA_MAX + 1];

static struct conf_fcb my_conf = {
    .cf_fcb.f_magic = 0xc09f6e5e,
    .cf_fcb.f_sectors = conf_fcb_area
};
#endif

#define DEFAULT_MBUF_MPOOL_BUF_LEN (512)
#define DEFAULT_MBUF_MPOOL_NBUFS (8)

uint8_t default_mbuf_mpool_data[DEFAULT_MBUF_MPOOL_BUF_LEN *
    DEFAULT_MBUF_MPOOL_NBUFS];

struct os_mbuf_pool default_mbuf_pool;
struct os_mempool default_mbuf_mpool;

static int net_cli(int argc, char **argv);
struct shell_cmd net_test_cmd = {
    .sc_cmd = "net",
    .sc_cmd_func = net_cli
};

#ifdef NFFS_PRESENT
static void
setup_for_nffs(void)
{
    /* NFFS_AREA_MAX is defined in the BSP-specified bsp.h header file. */
    struct nffs_area_desc descs[NFFS_AREA_MAX + 1];
    int cnt;
    int rc;

    /* Initialize nffs's internal state. */
    rc = nffs_init();
    assert(rc == 0);

    /* Convert the set of flash blocks we intend to use for nffs into an array
     * of nffs area descriptors.
     */
    cnt = NFFS_AREA_MAX;
    rc = flash_area_to_nffs_desc(FLASH_AREA_NFFS, &cnt, descs);
    assert(rc == 0);

    /* Attempt to restore an existing nffs file system from flash. */
    if (nffs_detect(descs) == FS_ECORRUPT) {
        /* No valid nffs instance detected; format a new one. */
        rc = nffs_format(descs);
        assert(rc == 0);
    }

    fs_mkdir(MY_CONFIG_DIR);
    rc = conf_file_src(&my_conf);
    assert(rc == 0);
    rc = conf_file_dst(&my_conf);
    assert(rc == 0);
}

#elif FCB_PRESENT

static void
setup_for_fcb(void)
{
    int cnt;
    int rc;

    rc = flash_area_to_sectors(FLASH_AREA_NFFS, &cnt, NULL);
    assert(rc == 0);
    assert(cnt <= sizeof(conf_fcb_area) / sizeof(conf_fcb_area[0]));
    flash_area_to_sectors(FLASH_AREA_NFFS, &cnt, conf_fcb_area);

    my_conf.cf_fcb.f_sector_cnt = cnt;

    rc = conf_fcb_src(&my_conf);
    assert(rc == 0);
    rc = conf_fcb_dst(&my_conf);
    assert(rc == 0);
}

#endif

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
        inet_def_service_init(NET_SERVICE_PRIO, net_service_stack,
          NET_SERVICE_STACK_SIZE);
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

    conf_init();

    os_init();

    rc = os_mempool_init(&default_mbuf_mpool, DEFAULT_MBUF_MPOOL_NBUFS,
            DEFAULT_MBUF_MPOOL_BUF_LEN, default_mbuf_mpool_data,
            "default_mbuf_data");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&default_mbuf_pool, &default_mbuf_mpool,
            DEFAULT_MBUF_MPOOL_BUF_LEN, DEFAULT_MBUF_MPOOL_NBUFS);
    assert(rc == 0);

    rc = os_msys_register(&default_mbuf_pool);
    assert(rc == 0);

    rc = hal_flash_init();
    assert(rc == 0);

#ifdef NFFS_PRESENT
    setup_for_nffs();
#elif FCB_PRESENT
    setup_for_fcb();
#endif

    shell_task_init(SHELL_TASK_PRIO, shell_stack, SHELL_TASK_STACK_SIZE,
                    SHELL_MAX_INPUT_LEN);

    console_printf("Wifi test\n");
    conf_load();

    wifi_task_init(WIFI_TASK_PRIO, wifi_stack, WIFI_STACK_SZ);
    winc1500_init();
    shell_cmd_register(&net_test_cmd);

    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return rc;
}

