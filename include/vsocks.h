/* ------------------------------------------------------------------
 * V-Socks - Proxy Task Header File
 * ------------------------------------------------------------------ */

#ifndef VSOCKS_H
#define VSOCKS_H

#include "defs.h"
#include "config.h"

#define L_ACCEPT                    3

#define LEVEL_AWAITING              1
#define LEVEL_SOCKS_VER             3
#define LEVEL_SOCKS_REQ             4

/**
 * Data queue structure
 */
struct queue_t
{
    size_t len;
    uint8_t arr[DATA_QUEUE_CAPACITY];
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
    size_t stream_size;
    int verbose;
    int epoll_fd;
    struct stream_t *stream_head;
    struct stream_t *stream_tail;
    struct stream_t stream_pool[POOL_SIZE];

    struct sockaddr_storage entrance;
    struct sockaddr_storage socks5;
};

/**
 * Proxy task entry point
 */
extern int proxy_task ( struct proxy_t *params );

#include "util.h"

#endif
