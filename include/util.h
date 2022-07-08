/* ------------------------------------------------------------------
 * Proxy Util - Header File
 * ------------------------------------------------------------------ */

#ifndef PROXY_UTIL_H
#define PROXY_UTIL_H

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <stdint.h>
#include <unistd.h>

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#include "config.h"

/**
 * Constants Definitions
 */
#define S_INVALID                   -1
#define S_PORT_A                    100
#define S_PORT_B                    200
#define LEVEL_NONE                  0
#define LEVEL_CONNECTING            111
#define LEVEL_FORWARDING            123
#define EPOLLREF                    ((struct pollfd*) -1)
#define STRADDR_SIZE                (INET_ADDRSTRLEN + INET6_ADDRSTRLEN + 16)

/**
 * Message Logging
 */
#ifdef STRIP_STRINGS
#define info(X)
#define failure(X)
#define verbose(X)
#else
#define info(...)  \
    printf("[" PROGRAM_SHORTCUT "] " __VA_ARGS__);
#define failure(...)  \
    fprintf(stderr, "[" PROGRAM_SHORTCUT "] " __VA_ARGS__);
#define verbose(...) \
    if (proxy->verbose) \
    { \
        printf("[" PROGRAM_SHORTCUT "] " __VA_ARGS__); \
    }
#endif

#define POLL_EVENTS_TO_4xSTR(EVENTS) \
    (EVENTS & POLLIN) ? "IN " : "", \
    (EVENTS & POLLOUT) ? "OUT " : "", \
    (EVENTS & POLLERR) ? "ERR " : "", \
    (EVENTS & POLLHUP) ? "HUP" : ""

#define EPOLL_EVENTS_TO_4xSTR(EVENTS) \
    (EVENTS & EPOLLIN) ? "IN " : "", \
    (EVENTS & EPOLLOUT) ? "OUT " : "", \
    (EVENTS & EPOLLERR) ? "ERR " : "", \
    (EVENTS & EPOLLHUP) ? "HUP" : ""

#ifdef PROXY_UTIL_BASE_STRUCTS

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

    /* additional params here */
};

/**
 * Proxy base structure
 */
struct proxy_t
{
    size_t stream_size;
    int verbose;
    int epoll_fd;
    struct stream_t *stream_head;
    struct stream_t *stream_tail;
    struct stream_t stream_pool[POOL_SIZE];

    /* additional params here */
};

/**
 * Handle stream events
 */
extern int handle_stream_events ( struct proxy_t *proxy, struct stream_t *stream );

#endif
/* ------------------------------------------------------------------
 * Proxy Util - Source File
 * ------------------------------------------------------------------ */

/* NOTE: Netowrk Address Related Functions */

/**
 * Find last character occurence in the string
 */

/**
 * Decode ip address and port number
 */
extern int ip_port_decode ( const char *input, struct sockaddr_storage *saddr );

/**
 * Format IP address adn port to string
 */
extern void format_ip_port ( const struct sockaddr_storage *saddr, char *buffer, size_t size );

/* NOTE: Socket Related Functions */

/**
 * Connect remote endpoint asynchronously
 */
extern int connect_async ( struct proxy_t *proxy, const struct sockaddr_storage *saddr );

/**
 * Bind address to listen socket
 */
extern int listen_socket ( struct proxy_t *proxy, const struct sockaddr_storage *saddr );

/**
 * Check for socket error
 */
extern int socket_has_error ( int sock );

/**
 * Set socket non-blocking mode
 */
extern int socket_set_nonblocking ( struct proxy_t *proxy, int sock );

/**
 * Forward data between sockets
 */
extern int socket_forward_data ( struct proxy_t *proxy, int srcfd, int dstfd );

/**
 * Shutdown and close the socket
 */
extern void shutdown_then_close ( struct proxy_t *proxy, int sock );

/* NOTE: Data Queue Related Functions */

/**
 * Reset data queue content
 */
extern void queue_reset ( struct queue_t *queue );

/**
 * Push bytes into data queue
 */
extern int queue_push ( struct queue_t *queue, const uint8_t * bytes, size_t len );

/**
 * Set data queue content
 */
extern int queue_set ( struct queue_t *queue, const uint8_t * bytes, size_t len );

/**
 * Shift bytes from data queue
 */
extern int queue_shift ( struct queue_t *queue, int fd );

/**
 * Assert minimum data length
 */
extern int check_enough_data ( struct proxy_t *proxy, struct stream_t *stream, size_t value );

/* NOTE: Event Listenning Related Functions */

/**
 * Setup proxy events listenning
 */
extern int proxy_events_setup ( struct proxy_t *proxy );

/**
 * Build stream event list with poll
 */
extern int build_poll_list ( struct proxy_t *proxy, struct pollfd *poll_list, size_t *poll_len );

/**
 * Update streams revents with poll
 */
extern void update_revents_poll ( struct proxy_t *proxy );

/**
 * Watch stream events with poll
 */
extern int watch_streams_poll ( struct proxy_t *proxy );

/**
 * Convert poll to epoll events
 */
extern int poll_to_epoll_events ( int poll_events );

/**
 * Convert epoll to poll events
 */
extern int epoll_to_poll_events ( int epoll_events );

/**
 * Build stream event list with epoll
 */
extern int build_epoll_list ( struct proxy_t *proxy );

/**
 * Update streams revents with poll
 */
extern void update_revents_epoll ( struct proxy_t *proxy, int nfds, struct epoll_event *events );

/**
 * Watch stream events with epoll
 */
extern int watch_streams_epoll ( struct proxy_t *proxy );

/**
 * Watch stream events
 */
extern int watch_streams ( struct proxy_t *proxy );

/* NOTE: Stream Related Functions */

/**
 * Insert new stream structure into the list
 */
extern struct stream_t *insert_stream ( struct proxy_t *proxy, int sock );

/**
 * Accept a new stream
 */
extern struct stream_t *accept_new_stream ( struct proxy_t *proxy, int lfd );

/**
 * Handle stream data forward
 */
extern int handle_forward_data ( struct proxy_t *proxy, struct stream_t *stream );

/**
 * Show relations statistics
 */
extern void show_stats ( struct proxy_t *proxy );

/**
 * Free and remove a single stream from the list
 */
extern void remove_stream ( struct proxy_t *proxy, struct stream_t *stream );

/*
 * Abandon associated pair of streams
 */
extern void remove_relation ( struct stream_t *stream );

/**
 * Remove all relations
 */
extern void remove_all_streams ( struct proxy_t *proxy );

/**
 * Remove pending streams
 */
extern void remove_pending_streams ( struct proxy_t *proxy );

/**
 * Remove abandoned streams
 */
extern void cleanup_streams ( struct proxy_t *proxy );

/**
 * Remove oldest forwarding relation
 */
extern void force_cleanup ( struct proxy_t *proxy, const struct stream_t *excl );

/**
 * Stream event handling cycle
 */
extern int handle_streams_cycle ( struct proxy_t *proxy );

#endif
