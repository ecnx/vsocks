/* ------------------------------------------------------------------
 * V-Socks - Project Config Header
 * ------------------------------------------------------------------ */

#ifndef S_PROXY_CONFIG_H
#define S_PROXY_CONFIG_H

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#ifdef VERBOSE_MODE
#define V(X) X
#else
#define V(X)
#endif

#define PROXY_STATUS_STOPPED 0
#define PROXY_STATUS_PENDING 1
#define PROXY_STATUS_RUNNING 2
#define PROXY_STATUS_FALLBACK 3

#define CONN_TIMEOUT_SEC 4
#define SEND_TIMEOUT_SEC 4
#define RECV_TIMEOUT_SEC 4

#define VSOCKS_VERSION              "1.04.2a"
#define POOL_SIZE                   256
#define LISTEN_BACKLOG              4
#define POLL_TIMEOUT_MSEC           16000
#define FORWARD_CHUNK_LEN           16384
#define BLOCK_LOCALHOST_PORTS       1
#define HTTPS_TRAFFIC_ONLY          0

#endif
