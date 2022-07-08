/* ------------------------------------------------------------------
 * V-Socks - Proxy Task Source Code
 * ------------------------------------------------------------------ */

#include "vsocks.h"
#include <linux/netfilter_ipv4.h>

/**
 * Estabilish connection with endpoint
 */
static int setup_endpoint_stream ( struct proxy_t *proxy, struct stream_t *stream,
    const struct sockaddr_storage *saddr )
{
    int sock;
    struct stream_t *neighbour;

    /* Connect remote endpoint asynchronously */
    if ( ( sock = connect_async ( proxy, saddr ) ) < 0 )
    {
        return sock;
    }

    /* Try allocating neighbour stream */
    if ( !( neighbour = insert_stream ( proxy, sock ) ) )
    {
        force_cleanup ( proxy, stream );
        neighbour = insert_stream ( proxy, sock );
    }

    /* Check for neighbour stream */
    if ( !neighbour )
    {
        shutdown_then_close ( proxy, sock );
        return -2;
    }

    /* Set neighbour role */
    neighbour->role = S_PORT_B;
    neighbour->level = LEVEL_CONNECTING;
    neighbour->events = POLLIN | POLLOUT;

    /* Build up a new relation */
    neighbour->neighbour = stream;
    stream->neighbour = neighbour;

    verbose ( "new relation between socket:%i and socket:%i\n", stream->fd, sock );

    return 0;
}

/**
 * Handle new stream creation
 */
static int handle_new_stream ( struct proxy_t *proxy, struct stream_t *stream )
{
    int status;
    struct stream_t *util;

    if ( ~stream->revents & POLLIN )
    {
        return -1;
    }

    /* Accept incoming connection */
    if ( !( util = accept_new_stream ( proxy, stream->fd ) ) )
    {
        return -2;
    }

    /* Setup new stream */
    util->role = S_PORT_A;
    util->level = LEVEL_AWAITING;
    util->events = 0;

    /* Setup endpoint stream */
    if ( ( status = setup_endpoint_stream ( proxy, util, &proxy->socks5 ) ) < 0 )
    {
        remove_stream ( proxy, util );
        return status;
    }

    return 0;
}

/**
 * Obtain original address and port from iptables redirect
 */
static int get_original_dest ( int sock, struct sockaddr_storage *saddr )
{
    socklen_t addrlen = sizeof ( struct sockaddr_storage );

    /* Clear original address */
    memset ( saddr, '\0', sizeof ( struct sockaddr_storage ) );

    /* Query original address and port */
    if ( getsockopt ( sock, SOL_IP, SO_ORIGINAL_DST, saddr, &addrlen ) < 0 )
    {
        failure ( "cannot get original destination (%i) using socket:%i\n", errno, sock );
        return -1;
    }

    return 0;
}

/**
 * Handle stream socks handshake and request
 */
static int handle_stream_socks ( struct proxy_t *proxy, struct stream_t *stream )
{
    size_t len;
    struct sockaddr_storage saddr;
    struct sockaddr_in *saddr_in;
    struct sockaddr_in6 *saddr_in6;
    char straddr[STRADDR_SIZE];
    uint8_t arr[DATA_QUEUE_CAPACITY];


    /* Expect socket ready to be read */
    if ( stream->revents & POLLIN )
    {
        /* Receive data chunk */
        if ( ( ssize_t ) ( len = recv ( stream->fd, arr, sizeof ( arr ), 0 ) ) < 2 )
        {
            failure ( "cannot receive data (%i) from socket:%i\n", errno, stream->fd );
            return -1;
        }

        /* Print progress */
        verbose ( "received %i byte(s) in handshake from socket:%i\n", ( int ) len, stream->fd );

        /* Enqueue input data */
        if ( queue_push ( &stream->queue, arr, len ) < 0 )
        {
            return -1;
        }
    }

    switch ( stream->level )
    {
    case LEVEL_CONNECTING:
        if ( stream->revents & POLLOUT )
        {
            /* Print current stage */
            verbose ( "processing socks CLIENT/VERSION stage on socket:%i...\n", stream->fd );

            /* Prepare request */
            arr[0] = 5; /* SOCKS5 version */
            arr[1] = 1; /* One auth method */
            arr[2] = 0; /* No auth method */

            /* Enqueue request */
            if ( queue_set ( &stream->queue, arr, 3 ) < 0 )
            {
                return -1;
            }

            /* Update levels and events flags */
            stream->level = LEVEL_SOCKS_VER;
            stream->events = POLLOUT;
        }
        break;
    case LEVEL_SOCKS_VER:
        if ( stream->revents & POLLIN )
        {
            /* Print current stage */
            verbose ( "verifying socks CLIENT/VERSION stage on socket:%i...\n", stream->fd );

            /* Assert minimum data length */
            if ( check_enough_data ( proxy, stream, 2 ) < 0 )
            {
                return 0;
            }

            /* Expect SOCKS5 version */
            if ( stream->queue.arr[0] != 5 )
            {
                failure ( "invalid socks version (0x%.2x) on socket:%i\n", stream->queue.arr[0],
                    stream->fd );
                return -1;
            }

            /* Expect no auth method */
            if ( stream->queue.arr[1] != 0 )
            {
                failure ( "invalid socks auth method (0x%.2x) on socket:%i\n", stream->queue.arr[1],
                    stream->fd );
                return -1;
            }

            /* Print current stage */
            verbose ( "completed socks CLIENT/VERSION stage on socket:%i\n", stream->fd );

            /* Print current stage */
            verbose ( "processing socks CLIENT/REQUEST stage on socket:%i...\n", stream->fd );

            /* Get destiantion host and port */
            if ( get_original_dest ( stream->neighbour->fd, &saddr ) < 0 )
            {
                return -1;
            }

            if ( proxy->verbose )
            {
                format_ip_port ( &saddr, straddr, sizeof ( straddr ) );
            }

            verbose ( "will connect (%s) via socks proxy with socket:%i...\n", straddr,
                stream->fd );

            switch ( saddr.ss_family )
            {
            case AF_INET:
                saddr_in = ( struct sockaddr_in * ) &saddr;
                /* Prepare request */
                arr[0] = 5;     /* SOCKS5 version */
                arr[1] = 1;     /* TCP/IP stream */
                arr[2] = 0;     /* Reserved */
                arr[3] = 1;     /* Connect IPv4 */
                memcpy ( arr + 4, &saddr_in->sin_addr, 4 );     /* IP 1st - 4th byte */
                arr[8] = ntohs ( saddr_in->sin_port ) >> 8;     /* Port 1st byte */
                arr[9] = ntohs ( saddr_in->sin_port ) & 0xff;   /* Port 2nd byte */
                len = 10;
                break;
            case AF_INET6:
                saddr_in6 = ( struct sockaddr_in6 * ) &saddr;
                /* Prepare request */
                arr[0] = 5;     /* SOCKS5 version */
                arr[1] = 1;     /* TCP/IP stream */
                arr[2] = 0;     /* Reserved */
                arr[3] = 4;     /* Connect IPv6 */
                memcpy ( arr + 4, &saddr_in6->sin6_addr, 16 );  /* IP 1st - 4th byte */
                arr[20] = ntohs ( saddr_in6->sin6_port ) >> 8;  /* Port 1st byte */
                arr[21] = ntohs ( saddr_in6->sin6_port ) & 0xff;        /* Port 2nd byte */
                len = 22;
                break;
            default:
                failure ( "invalid socket family (%i) on socket:%i\n", saddr.ss_family,
                    stream->fd );
                return -1;
            }

            /* Enqueue request */
            if ( queue_set ( &stream->queue, arr, 10 ) < 0 )
            {
                return -1;
            }

            /* Update levels and events flags */
            stream->level = LEVEL_SOCKS_REQ;
            stream->events = POLLOUT;
        }
        break;
    case LEVEL_SOCKS_REQ:
        if ( stream->revents & POLLIN )
        {
            /* Print current stage */
            verbose ( "verifying socks CLIENT/REQUEST stage on socket:%i...\n", stream->fd );

            /* Assert minimum data length */
            if ( check_enough_data ( proxy, stream, 2 ) < 0 )
            {
                return 0;
            }

            /* Expect SOCKS5 version */
            if ( stream->queue.arr[0] != 5 )
            {
                failure ( "invalid socks version (0x%.2x) on socket:%i\n", stream->queue.arr[0],
                    stream->fd );
                return -1;
            }

            /* Expect status success */
            if ( stream->queue.arr[1] != 0 )
            {
                failure ( "invalid socks status (0x%.2x) on socket:%i\n", stream->queue.arr[1],
                    stream->fd );
                return -1;
            }

            /* Print current stage */
            verbose ( "completed socks CLIENT/REQUEST stage on socket:%i\n", stream->fd );

            /* Update levels and events flags */
            stream->level = LEVEL_FORWARDING;
            stream->events = POLLIN;
            stream->neighbour->level = LEVEL_FORWARDING;
            stream->neighbour->events = POLLIN;
        }
        break;
    default:
        return -1;
    }

    return 0;
}

/**
 * Handle stream events
 */
int handle_stream_events ( struct proxy_t *proxy, struct stream_t *stream )
{
    int status;

    if ( handle_forward_data ( proxy, stream ) >= 0 )
    {
        return 0;
    }

    if ( stream->role == S_PORT_B && stream->queue.len && ( stream->revents & POLLOUT ) )
    {
        if ( queue_shift ( &stream->queue, stream->fd ) < 0 )
        {
            remove_relation ( stream );
            return 0;
        }
        if ( stream->queue.len == 0 )
        {
            stream->events = POLLIN;
        }
        return 0;
    }

    switch ( stream->role )
    {
    case L_ACCEPT:
        show_stats ( proxy );
        if ( handle_new_stream ( proxy, stream ) == -2 )
        {
            return -1;
        }
        return 0;
    case S_PORT_B:
        if ( ( status = handle_stream_socks ( proxy, stream ) ) >= 0 )
        {
            return 0;
        }
        break;
    }

    remove_relation ( stream );

    return 0;
}


/**
 * Proxy task entry point
 */
int proxy_task ( struct proxy_t *proxy )
{
    int status = 0;
    int sock;
    struct stream_t *stream;

    /* Set stream size */
    proxy->stream_size = sizeof ( struct stream_t );

    /* Reset current state */
    proxy->stream_head = NULL;
    proxy->stream_tail = NULL;
    memset ( proxy->stream_pool, '\0', sizeof ( proxy->stream_pool ) );

    /* Proxy events setup */
    if ( proxy_events_setup ( proxy ) < 0 )
    {
        return -1;
    }

    /* Setup listen socket */
    if ( ( sock = listen_socket ( proxy, &proxy->entrance ) ) < 0 )
    {
        if ( proxy->epoll_fd >= 0 )
        {
            close ( proxy->epoll_fd );
        }
        return -1;
    }

    /* Allocate new stream */
    if ( !( stream = insert_stream ( proxy, sock ) ) )
    {
        shutdown_then_close ( proxy, sock );
        if ( proxy->epoll_fd >= 0 )
        {
            close ( proxy->epoll_fd );
        }
        return -1;
    }

    /* Update listen stream */
    stream->role = L_ACCEPT;
    stream->events = POLLIN;

    verbose ( "proxy setup was successful\n" );

    /* Run forward loop */
    while ( ( status = handle_streams_cycle ( proxy ) ) >= 0 );

    /* Do not close reset pipe */
    stream->fd = -1;

    /* Remove all streams */
    remove_all_streams ( proxy );

    /* Close epoll fd if created */
    if ( proxy->epoll_fd >= 0 )
    {
        close ( proxy->epoll_fd );
        proxy->epoll_fd = -1;
    }

    verbose ( "done proxy uninitializing\n" );

    return status;
}
