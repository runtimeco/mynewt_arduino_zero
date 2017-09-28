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
#include <inttypes.h>
#include <assert.h>
#include <string.h>

#include <os/os.h>
#include <os/endian.h>
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>

#include <mn_socket/mn_socket.h>
#include <mn_socket/mn_socket_ops.h>

#include "winc1500/bsp/nm_bsp.h"
#include "winc1500/socket/socket.h"
#include "winc1500_priv.h"
#include "driver/m2m_hif.h"

#ifdef SOCK_DEBUG
#define DEBUG_PRINTF console_printf
#else
#define DEBUG_PRINTF(...)
#endif

static int winc1500_sock_create(struct mn_socket **sp, uint8_t domain,
  uint8_t type, uint8_t proto);
static int winc1500_sock_close(struct mn_socket *);
static int winc1500_sock_connect(struct mn_socket *, struct mn_sockaddr *);
static int winc1500_sock_bind(struct mn_socket *, struct mn_sockaddr *);
static int winc1500_sock_listen(struct mn_socket *, uint8_t qlen);
static int winc1500_sock_sendto(struct mn_socket *, struct os_mbuf *,
  struct mn_sockaddr *);
static int winc1500_sock_recvfrom(struct mn_socket *, struct os_mbuf **,
  struct mn_sockaddr *);
static int winc1500_sock_setsockopt(struct mn_socket *, uint8_t level,
  uint8_t name, void *val);
static int winc1500_sock_getpeername(struct mn_socket *, struct mn_sockaddr *);
static int winc1500_itf_getnext(struct mn_itf *);
static int winc1500_itf_addr_getnext(struct mn_itf *, struct mn_itf_addr *);

/*
 * MAX_SOCKET comes from socket/socket.h
 *
 * WINC1500 identifies sockets with a number from 0 - MAX_SOCKETS.
 * We use that as an index to this array when notifications arrive.
 *
 * WINC1500 supports IPv4 only with it's internal IP stack.
 * It probably would be able to operate in IPv6 environment if we used their
 * 'raw' ethernet interface.
 */
static struct winc1500_sock {
    struct mn_socket ws_sock;       /* for mn_socket API */
    struct mn_sockaddr_in ws_tgt;   /* target address for connected sockets */
    struct os_sem ws_sem;           /* used with sync calls to make app sleep */
    uint8_t ws_idx;                 /* socket id (array index) */
    uint8_t ws_waiting:1;           /* set if waiting on semaphore */
    uint8_t ws_poll:1;              /* whether should be polled for data */
    uint8_t ws_closed:1;            /* if we know that remote has closed */
    uint8_t ws_err;                 /* err return for sync calls */
    uint8_t ws_type;                /* SOCK_DGRAM/SOCK_STREAM */
    STAILQ_HEAD(, os_mbuf_pkthdr) ws_rx; /* RX data queue */
    struct os_mbuf *ws_tx;          /* SOCK_STREAM, data being TX'd */
} winc1500_socks[MAX_SOCKET];

#define WINC1500_SOCK_RX_SIZE       1500

/*
 * State of socket RX polling. This is done periodically.
 */
static struct winc1500_sock_state {
    uint8_t polling;                /* if polling is going on */
    struct os_mbuf *rx_buf;         /* chain of mbufs to receive data to */
    struct os_mbuf *cur_buf;        /* currently receiving to this */
    uint8_t tx_buf[WINC1500_SOCK_RX_SIZE]; /* send buffer */
} winc1500_socket_state;

static const struct mn_socket_ops winc1500_sock_ops = {
    .mso_create = winc1500_sock_create,
    .mso_close = winc1500_sock_close,

    .mso_bind = winc1500_sock_bind,
    .mso_connect = winc1500_sock_connect,
    .mso_listen = winc1500_sock_listen,

    .mso_sendto = winc1500_sock_sendto,
    .mso_recvfrom = winc1500_sock_recvfrom,

    .mso_setsockopt = winc1500_sock_setsockopt,

    .mso_getpeername = winc1500_sock_getpeername,

    .mso_itf_getnext = winc1500_itf_getnext,
    .mso_itf_addr_getnext = winc1500_itf_addr_getnext
};

/*
 * Conversion from WINC1500 error codes to mn_socket error codes.
 */
static int
winc1500_err_to_mn_err(int err)
{
    switch (err) {
    case SOCK_ERR_NO_ERROR:
        return 0;
    case SOCK_ERR_INVALID_ADDRESS:
        return MN_EINVAL;
    case SOCK_ERR_ADDR_ALREADY_IN_USE:
        return MN_EADDRINUSE;
    case SOCK_ERR_MAX_TCP_SOCK:
    case SOCK_ERR_MAX_UDP_SOCK:
        return MN_ENOBUFS;
    case SOCK_ERR_INVALID_ARG:
        return MN_EINVAL;
    case SOCK_ERR_MAX_LISTEN_SOCK:
        return MN_ENOBUFS;
    case SOCK_ERR_INVALID:
        return MN_EINVAL;
    case SOCK_ERR_ADDR_IS_REQUIRED:
        return MN_EDESTADDRREQ;
    case SOCK_ERR_CONN_ABORTED:
        return MN_ECONNABORTED;
    case SOCK_ERR_TIMEOUT:
        return MN_ETIMEDOUT;
    case SOCK_ERR_BUFFER_FULL:
        return MN_ENOBUFS;
    default:
        return MN_EUNKNOWN;
    }
}

/*
 * Conversion between WINC1500 sockaddr and mn_socket sockaddr.
 */
static int
winc1500_mn_addr_to_addr(struct mn_sockaddr_in *msin, struct sockaddr_in *sin)
{
    if (msin->msin_len != sizeof(*msin) || msin->msin_family != MN_AF_INET) {
        return MN_EINVAL;
    }
    sin->sin_family = AF_INET;
    sin->sin_port = msin->msin_port;
    sin->sin_addr.s_addr = msin->msin_addr.s_addr;

    return 0;
}

static void
winc1500_addr_to_mn_addr(struct sockaddr_in *sin, struct mn_sockaddr_in *msin)
{
    msin->msin_len = sizeof(*msin);
    msin->msin_family = MN_AF_INET;
    msin->msin_port = sin->sin_port;
    msin->msin_addr.s_addr = sin->sin_addr.s_addr;
}

/*
 * Wait for response from WINC1500 for operations which are synchronous
 * to app. E.g. mn_bind() needs to wait before returning to app.
 */
static int
winc1500_sock_wait(struct winc1500_sock *s, uint32_t ticks)
{
    int rc;

    s->ws_waiting = 1;
    os_mutex_release(&winc1500.w_if.wi_mtx);

    rc = os_sem_pend(&s->ws_sem, ticks);

    os_mutex_pend(&winc1500.w_if.wi_mtx, OS_TIMEOUT_NEVER);
    s->ws_waiting = 0;
    if (rc == OS_TIMEOUT) {
        return MN_ETIMEDOUT;
    } else {
        rc = s->ws_err;
        s->ws_err = 0;
        return winc1500_err_to_mn_err(rc);
    }
}

/*
 * Response came. If app is still waitign, wake it up.
 */
static void
winc1500_sock_wake(struct winc1500_sock *s, uint8_t err)
{
    if (s->ws_waiting) {
        s->ws_err = err;
        os_sem_release(&s->ws_sem);
    }
}

static int
winc1500_sock_create(struct mn_socket **sp, uint8_t domain, uint8_t type,
  uint8_t proto)
{
    int idx;
    struct winc1500_sock *ws;

    if (domain != MN_PF_INET) {
        return MN_EAFNOSUPPORT;
    }
    switch (type) {
    case MN_SOCK_DGRAM:
        type = SOCK_DGRAM;
        break;
    case MN_SOCK_STREAM:
        type = SOCK_STREAM;
        break;
    case 0:
        break;
    default:
        return MN_EPROTONOSUPPORT;
    }
    os_mutex_pend(&winc1500.w_if.wi_mtx, OS_TIMEOUT_NEVER);
    idx = socket(AF_INET, type, 0);
    if (idx >= 0) {
        ws = &winc1500_socks[idx];
        os_sem_init(&ws->ws_sem, 0);
        memset(&ws->ws_tgt, 0, sizeof(ws->ws_tgt));
        ws->ws_waiting = 0;
        ws->ws_err = 0;
        ws->ws_type = type;
        *sp = &ws->ws_sock;
    }
    os_mutex_release(&winc1500.w_if.wi_mtx);
    if (idx < 0 ) {
        return MN_ENOBUFS;
    } else {
        return 0;
    }
}

static int
winc1500_sock_close(struct mn_socket *sock)
{
    struct winc1500_sock *ws = (struct winc1500_sock *)sock;
    struct os_mbuf_pkthdr *m;

    os_mutex_pend(&winc1500.w_if.wi_mtx, OS_TIMEOUT_NEVER);
    close(ws->ws_idx);
    ws->ws_type = 0;
    ws->ws_poll = 0;
    ws->ws_closed = 0;

    /*
     * When socket is closed, we must free all mbufs which might be
     * queued in it.
     */
    while ((m = STAILQ_FIRST(&ws->ws_rx))) {
        STAILQ_REMOVE_HEAD(&ws->ws_rx, omp_next);
        os_mbuf_free_chain(OS_MBUF_PKTHDR_TO_MBUF(m));
    }
    if (ws->ws_tx) {
        os_mbuf_free_chain(ws->ws_tx);
        ws->ws_tx = NULL;
    }
    os_mutex_release(&winc1500.w_if.wi_mtx);
    return 0;
}

static int
winc1500_sock_bind(struct mn_socket *sock, struct mn_sockaddr *addr)
{
    struct winc1500_sock *ws = (struct winc1500_sock *)sock;
    struct sockaddr_in sin;
    int rc;

    rc = winc1500_mn_addr_to_addr((struct mn_sockaddr_in *)addr, &sin);
    if (rc) {
        return rc;
    }
    os_mutex_pend(&winc1500.w_if.wi_mtx, OS_TIMEOUT_NEVER);
    rc = bind(ws->ws_idx, (struct sockaddr *)&sin, sizeof(sin));
    if (rc == 0) {
        rc = winc1500_sock_wait(ws, OS_TICKS_PER_SEC);
    } else {
        rc = winc1500_err_to_mn_err(rc);
    }
    os_mutex_release(&winc1500.w_if.wi_mtx);
    return rc;
}

static int
winc1500_sock_connect(struct mn_socket *sock, struct mn_sockaddr *addr)
{
    struct sockaddr_in sin;
    struct winc1500_sock *ws = (struct winc1500_sock *)sock;
    int rc;

    rc = winc1500_mn_addr_to_addr((struct mn_sockaddr_in *)addr, &sin);
    if (rc) {
        return rc;
    }
    ws->ws_tgt = *(struct mn_sockaddr_in *)addr;
    os_mutex_pend(&winc1500.w_if.wi_mtx, OS_TIMEOUT_NEVER);
    if (ws->ws_type == SOCK_STREAM) {
        rc = connect(ws->ws_idx, (struct sockaddr *)&sin, sizeof(sin));
    } else {
        /*
         * UDP socket. Docs talk about different kind fo bind? XXXX check
         */
        rc = SOCK_ERR_INVALID;
    }
    ws->ws_poll = 1;
    os_mutex_release(&winc1500.w_if.wi_mtx);
    return winc1500_err_to_mn_err(rc);
}

static int
winc1500_sock_listen(struct mn_socket *sock, uint8_t qlen)
{
    struct winc1500_sock *ws = (struct winc1500_sock *)sock;
    int rc;

    os_mutex_pend(&winc1500.w_if.wi_mtx, OS_TIMEOUT_NEVER);
    rc = listen(ws->ws_idx, qlen);
    if (rc == 0) {
        rc = winc1500_sock_wait(ws, OS_TICKS_PER_SEC);
    } else {
        rc = winc1500_err_to_mn_err(rc);
    }
    os_mutex_release(&winc1500.w_if.wi_mtx);
    return rc;
}

/*
 * TX routine for stream sockets (TCP). The data to send is pointed
 * by ws_tx.
 * Keep sending mbufs until WINC1500 says that it can't take anymore.
 * then wait for send event notification before continuing.
 */
static int
winc1500_stream_tx(struct winc1500_sock *ws, int notify)
{
    struct os_mbuf *m;
    struct os_mbuf *n;
    int rc;

    rc = 0;

    os_mutex_pend(&winc1500.w_if.wi_mtx, OS_TIMEOUT_NEVER);
    while (ws->ws_tx && rc == 0) {
        m = ws->ws_tx;
        n = SLIST_NEXT(m, om_next);
        rc = send(ws->ws_idx, m->om_data, m->om_len, 0);
        if (rc == 0) {
            ws->ws_tx = n;
            os_mbuf_free(m);
        }
    }
    os_mutex_release(&winc1500.w_if.wi_mtx);
    if (rc) {
        if (rc == MN_ENOBUFS) {
            rc = 0;
        } else {
            rc = winc1500_err_to_mn_err(rc);
        }
    }
    if (notify) {
        if (ws->ws_tx == NULL) {
            mn_socket_writable(&ws->ws_sock, 0);
        } else if (rc) {
            mn_socket_writable(&ws->ws_sock, rc);
        }
    }
    return rc;
}

static int
winc1500_sock_sendto(struct mn_socket *sock, struct os_mbuf *m,
  struct mn_sockaddr *dst)
{
    struct winc1500_sock_state *wss = &winc1500_socket_state;
    struct winc1500_sock *ws = (struct winc1500_sock *)sock;
    struct sockaddr_in sin;
    struct os_mbuf *o;
    int rc;
    int off;

    if (ws->ws_type == SOCK_STREAM) {
        if (dst) {
            return MN_EINVAL;
        }
        rc = 0;
        if (ws->ws_tx) {
            return MN_EAGAIN;
        }
        ws->ws_tx = m;

        /*
         * Send this data.
         */
        rc = winc1500_stream_tx(ws, 0);
    } else {
        /*
         * XXXX every write presumably sends a single datagram.
         * WINC1500 API takes a single pointer to data, along with length.
         * So we have to copy the mbuf chain into a contiguous area of
         * memory before calling sendto().
         */
        rc = winc1500_mn_addr_to_addr((struct mn_sockaddr_in *)dst, &sin);
        if (rc) {
            goto err;
        }

        os_mutex_pend(&winc1500.w_if.wi_mtx, OS_TIMEOUT_NEVER);
        off = 0;
        for (o = m; o; o = SLIST_NEXT(o, om_next)) {
            if (off + o->om_len > sizeof(wss->tx_buf)) {
                rc = MN_EINVAL;
                os_mutex_release(&winc1500.w_if.wi_mtx);
                goto err;
            }
            os_mbuf_copydata(o, 0, o->om_len, &wss->tx_buf[off]);
            off += o->om_len;
        }
        rc = sendto(ws->ws_idx, wss->tx_buf, off, 0,
          (struct sockaddr *)&sin, sizeof(sin));
        os_mutex_release(&winc1500.w_if.wi_mtx);
        if (rc) {
            rc = winc1500_err_to_mn_err(rc);
            goto err;
        }
    }
    if (rc == 0 && ws->ws_type == SOCK_DGRAM) {
        os_mbuf_free_chain(m);
        return 0;
    } else {
err:
        return rc;
    }
}

static int winc1500_sock_recvfrom(struct mn_socket *sock, struct os_mbuf **mp,
  struct mn_sockaddr *addr)
{
    struct winc1500_sock *ws = (struct winc1500_sock *)sock;
    struct os_mbuf_pkthdr *m;
    struct mn_sockaddr_in *msin = (struct mn_sockaddr_in *)addr;

    os_mutex_pend(&winc1500.w_if.wi_mtx, OS_TIMEOUT_NEVER);
    m = STAILQ_FIRST(&ws->ws_rx);
    if (m) {
        STAILQ_REMOVE_HEAD(&ws->ws_rx, omp_next);
        STAILQ_NEXT(m, omp_next) = NULL;
    }
    os_mutex_release(&winc1500.w_if.wi_mtx);
    if (m) {
        DEBUG_PRINTF("sock_recvfrom %d\n", ws->ws_idx);
        *mp = OS_MBUF_PKTHDR_TO_MBUF(m);
        if (addr) {
            if (ws->ws_type == SOCK_DGRAM) {
                /*
                 * UDP sockets have the packet source stashed inside mbuf
                 * user header.
                 */
                *msin = *(struct mn_sockaddr_in *)(m + 1);
            } else {
                /*
                 * For TCP, copy it from socket.
                 */
                *msin = ws->ws_tgt;
            }
        }
        return 0;
    } else {
        *mp = NULL;
        return MN_EAGAIN;
    }
}

static int
winc1500_sock_setsockopt(struct mn_socket *sock, uint8_t level, uint8_t name,
  void *val)
{
    struct winc1500_sock *ws = (struct winc1500_sock *)sock;
    struct mn_mreq *mreq;
    int rc;

    if (level == MN_SO_LEVEL) {
        switch (name) {
        case MN_MCAST_JOIN_GROUP:
        case MN_MCAST_LEAVE_GROUP:
            mreq = (struct mn_mreq *)val;
            if (mreq->mm_family != MN_AF_INET) {
                return MN_EINVAL;
            }
            if (name == MN_MCAST_JOIN_GROUP) {
                name = IP_ADD_MEMBERSHIP;
            } else {
                name = IP_DROP_MEMBERSHIP;
            }
            os_mutex_pend(&winc1500.w_if.wi_mtx, OS_TIMEOUT_NEVER);
            rc = setsockopt(ws->ws_idx, 1, name, &mreq->mm_addr.v4,
              sizeof(mreq->mm_addr.v4));
            os_mutex_release(&winc1500.w_if.wi_mtx);
            if (rc) {
                return winc1500_err_to_mn_err(rc);
            }
            return 0;
        case MN_MCAST_IF:
            return 0;
        }
    }
    return MN_EPROTONOSUPPORT;
}

static int
winc1500_sock_getpeername(struct mn_socket *sock, struct mn_sockaddr *addr)
{
    struct winc1500_sock *ws = (struct winc1500_sock *)sock;

    *(struct mn_sockaddr_in *)addr = ws->ws_tgt;
    return 0;
}

/*
 * There is only one interface, which can have only one address.
 */
static int
winc1500_itf_getnext(struct mn_itf *mi)
{
    if (mi->mif_name[0] != '\0') {
        return MN_EADDRNOTAVAIL;
    }
    strcpy(mi->mif_name, "wnc");
    mi->mif_idx = 1;
    mi->mif_flags = MN_ITF_F_MULTICAST;
    if (winc1500.w_up) {
        mi->mif_flags |= (MN_ITF_F_UP | MN_ITF_F_LINK);
    }
    return 0;
}

static int
winc1500_itf_addr_getnext(struct mn_itf *mi, struct mn_itf_addr *mia)
{
    if (mi->mif_idx != 1) {
        return MN_EADDRNOTAVAIL;
    }
    if (winc1500.w_up == 0) {
        return MN_EADDRNOTAVAIL;
    }
    if (mia->mifa_family) {
        return MN_EADDRNOTAVAIL;
    }
    mia->mifa_family = MN_AF_INET;
    mia->mifa_plen = winc1500.w_plen;
    mia->mifa_addr.v4.s_addr = winc1500.w_addr;
    return 0;
}

/*
 * Try to receive data for first suitable socket, starting from winc1500_socks
 * array index idx.
 * If there is a response, the mbuf allocated here is moved to socket's RX
 * queue.
 */
static int
winc1500_sock_start_rx(struct winc1500_sock_state *wss, int idx, int cont)
{
    struct winc1500_sock *ws;
    struct os_mbuf *m;
    struct os_mbuf *n;
    int need;
    int rc;

    wss->polling = 0;
    if (!wss->rx_buf) {
        /*
         * Allocate a chain of mbufs, with total available space
         * more than WINC1500_SOCK_RX_SIZE bytes.
         */
        m = os_msys_get_pkthdr(WINC1500_SOCK_RX_SIZE,
          sizeof(struct mn_sockaddr_in));
        if (!m) {
            goto flow_ctl;
        }
        wss->rx_buf = m;
        need = WINC1500_SOCK_RX_SIZE - OS_MBUF_TRAILINGSPACE(m);

        while (need > 0) {
            n = os_msys_get(need, 0);
            if (n) {
                SLIST_NEXT(m, om_next) = n;
                m = n;
                need -= OS_MBUF_TRAILINGSPACE(n);
            } else {
                os_mbuf_free_chain(wss->rx_buf);
                wss->rx_buf = NULL;
                goto flow_ctl;
            }
        }
    }

    /*
     * We have mbufs to receive a packet to.
     */
    hif_flow_ctrl(0);

    for ( ; idx < MAX_SOCKET; idx++) {
        ws = &winc1500_socks[idx];
        if (!ws->ws_poll) {
            continue;
        }

        if (!cont) {
            /*
             * Poll socket for data. Tell it to write the data to first mbuf
             * in the chain.
             */
            m = wss->rx_buf;
        } else {
            m = SLIST_NEXT(wss->cur_buf, om_next);
        }
        rc = recvfrom(idx, m->om_data, OS_MBUF_TRAILINGSPACE(m), 1);
        if (rc == 0) {
            wss->cur_buf = m;
            wss->polling = 1;
            return 0;
        } else {
            return -1;
        }
    }
    return 0;
flow_ctl:

    /*
     * We don't have mbuf to receive data to.
     */
    hif_flow_ctrl(1);
    return -1;
}

/*
 * Callback from WINC1500, giving us a socket notification.
 *
 * winc1500_mtx is already held when this is called.
 */
static void
winc1500_sock_cb(SOCKET idx, uint8_t msg, void *data)
{
    struct winc1500_sock_state *wss = &winc1500_socket_state;
    tstrSocketConnectMsg *connect;
    tstrSocketBindMsg *bind;
    tstrSocketListenMsg *listen;
    tstrSocketAcceptMsg *accept;
    tstrSocketRecvMsg *recv_msg;
    struct winc1500_sock *ws;
    struct winc1500_sock *new_ws;
    struct os_mbuf *m;
    int len;
    int cont;

    if (msg != SOCKET_MSG_RECV && msg != SOCKET_MSG_RECVFROM) {
        DEBUG_PRINTF("sock cb %d msg %d data %x\n", idx, msg, (int)data);
    }
    assert(idx < MAX_SOCKET);
    ws = &winc1500_socks[idx];
    switch (msg) {
    case SOCKET_MSG_BIND:
        bind = (tstrSocketBindMsg *)data;
        winc1500_sock_wake(ws, bind->status);
        if (ws->ws_type == SOCK_DGRAM) {
            ws->ws_poll = 1;
        }
        break;
    case SOCKET_MSG_LISTEN:
        listen = (tstrSocketListenMsg *)data;
        winc1500_sock_wake(ws, listen->status);
        break;
    case SOCKET_MSG_ACCEPT:
        accept = (tstrSocketAcceptMsg *)data;

        new_ws = &winc1500_socks[accept->sock];
        new_ws->ws_sock.ms_ops = &winc1500_sock_ops;
        winc1500_addr_to_mn_addr(&accept->strAddr, &new_ws->ws_tgt);
        os_sem_init(&new_ws->ws_sem, 0);
        new_ws->ws_err = 0;
        new_ws->ws_type = ws->ws_type;
        new_ws->ws_poll = 1;

#ifdef SOCK_DEBUG
        uint8_t *ip = (uint8_t *)&new_ws->ws_tgt.msin_addr;
        DEBUG_PRINTF(" newconn %d %d.%d.%d.%d/%d\n", accept->sock,
          ip[0], ip[1], ip[2], ip[3], ntohs(accept->strAddr.sin_port));
#endif
        if (mn_socket_newconn(&ws->ws_sock, &new_ws->ws_sock)) {
            /*
             * XXXX not accepting the connection. should close it
             */
        }
        break;
    case SOCKET_MSG_CONNECT:
        connect = (tstrSocketConnectMsg *)data;
        mn_socket_writable(&ws->ws_sock,
          winc1500_err_to_mn_err(connect->s8Error));
        break;
    case SOCKET_MSG_RECV:
    case SOCKET_MSG_RECVFROM:
        recv_msg = (tstrSocketRecvMsg *)data;
        len = recv_msg->s16BufferSize;
        cont = 0;
        if (len == SOCK_ERR_TIMEOUT) {
            idx = idx + 1;
        } else {
            DEBUG_PRINTF(" %s %d %d %x %x\n",
              msg == SOCKET_MSG_RECV ? "recv" : "recvfrom",
              recv_msg->s16BufferSize, recv_msg->u16RemainingSize,
              (int)recv_msg->pu8Buffer,
              (int)wss->rx_buf);
            if (len > 0) {
                assert(wss->cur_buf);
                m = wss->cur_buf;
                OS_MBUF_PKTLEN(wss->rx_buf) += len;
                m->om_len = len;
                if (ws->ws_type == SOCK_DGRAM && m == wss->rx_buf) {
                    winc1500_addr_to_mn_addr(&recv_msg->strRemoteAddr,
                      OS_MBUF_USRHDR(m));
                }
                if (recv_msg->u16RemainingSize) {
                    cont = 1;
                } else {
                    if (SLIST_NEXT(m, om_next)) {
                        os_mbuf_free_chain(SLIST_NEXT(m, om_next));
                        SLIST_NEXT(m, om_next) = NULL;
                    }
                    m = wss->rx_buf;
                    wss->rx_buf = NULL;
                    STAILQ_INSERT_TAIL(&ws->ws_rx, OS_MBUF_PKTHDR(m), omp_next);
                    mn_socket_readable(&ws->ws_sock, 0);
                }
            } else {
                idx = idx + 1;
                ws->ws_closed = 1;
                ws->ws_poll = 0;
                winc1500_sock_wake(ws, len);
                mn_socket_readable(&ws->ws_sock, winc1500_err_to_mn_err(len));
            }
        }
        winc1500_sock_start_rx(wss, idx, cont);
        break;
    case SOCKET_MSG_SEND:
        DEBUG_PRINTF(" send %d\n", *(sint16 *)data);
        if (ws->ws_type == SOCK_STREAM) {
            winc1500_stream_tx(ws, 1);
        }
        break;
    case SOCKET_MSG_SENDTO:
        DEBUG_PRINTF(" sendto\n");
        break;
    default:
        break;
    }
}

static void
winc1500_resolve_cb(uint8_t *name, uint32_t addr)
{
    console_printf("resolve cb %s addr %x\n", (char *)name, (int)addr);
}

void
winc1500_socket_start(void)
{
    socketInit();
    registerSocketCallback(winc1500_sock_cb, winc1500_resolve_cb);
}

void
winc1500_socket_poll(void)
{
    struct winc1500_sock_state *wss = &winc1500_socket_state;

    if (wss->polling) {
        return;
    }
    winc1500_sock_start_rx(wss, 0, 0);
}

int
winc1500_socket_init(void)
{
    int i;

    for (i = 0; i < sizeof(winc1500_socks) / sizeof(winc1500_socks[0]); i++) {
        winc1500_socks[i].ws_idx = i;
        STAILQ_INIT(&winc1500_socks[i].ws_rx);
    }
    return mn_socket_ops_reg(&winc1500_sock_ops);
}
