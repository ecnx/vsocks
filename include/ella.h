/* ------------------------------------------------------------------
 * Ella Proxy - Shared Project Header
 * ------------------------------------------------------------------ */

#include "config.h"

#ifndef ELLA_H
#define ELLA_H

#define L_ACCEPT                    1
#define S_PORT_A                    2
#define S_PORT_B                    3

#define ELLA_VERSION                "1.03.5a"

#define POLL_TIMEOUT_MSEC           16 * 1000
#define POLL_BASE_SIZE              32
#define LISTEN_BACKLOG              4

#define LEVEL_NONE                  0
#define LEVEL_AWAITING              1
#define LEVEL_CONNECT               2
#define LEVEL_FORWARD               3

/**
 * Ella Proxy program params
 */
struct ella_params_t
{
    unsigned int listen_addr;
    unsigned short listen_port;

    unsigned int socks_addr;
    unsigned short socks_port;
};

/**
 * Proxy task entry point
 */
extern int proxy_task ( const struct ella_params_t *params );

/**
 * Resolve hostname into IPv4 address
 */
extern int nsaddr ( const char *hostname, unsigned int *addr );

#endif
