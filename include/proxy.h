/* ------------------------------------------------------------------
 * V-Socks - Proxy Task Header File
 * ------------------------------------------------------------------ */

#ifndef PROXY_H
#define PROXY_H

#include "config.h"

#define S_INVALID                   -1
#define L_ACCEPT                    0
#define S_PORT_A                    1
#define S_PORT_B                    2

#define LEVEL_NONE                  0
#define LEVEL_AWAITING              1
#define LEVEL_CONNECTING            2
#define LEVEL_SOCKS_VER             3
#define LEVEL_SOCKS_REQ             4
#define LEVEL_SOCKS_PASS            5
#define LEVEL_FORWARDING            6

#define EPOLLREF                    ((struct pollfd*) -1)

/**
 * Utility data queue
 */
struct queue_t
{
    size_t len;
    unsigned char arr[16];
};

/**
 * IP/TCP connection stream
 */
struct stream_t
{
    int role;
    int fd;
    int level;
    int allocated;
    int abandoned;
    short events;
    short levents;
    short revents;

    struct pollfd *pollref;
    struct stream_t *neighbour;
    struct stream_t *prev;
    struct stream_t *next;
    struct queue_t queue;
};

/**
 * Proxy program params
 */
struct proxy_t
{
    int epoll_fd;
    unsigned int listen_addr;
    unsigned short listen_port;

    unsigned int socks5_addr;
    unsigned short socks5_port;

    unsigned int dest_addr;
    unsigned short dest_port;

    struct stream_t *stream_head;
    struct stream_t *stream_tail;
    struct stream_t stream_pool[POOL_SIZE];
};

/**
 * Proxy task entry point
 */
extern int proxy_task ( struct proxy_t *params );

#endif
