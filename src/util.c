/* ------------------------------------------------------------------
 * Proxy Util - Source File
 * ------------------------------------------------------------------ */

#define PROXY_UTIL_BASE_STRUCTS
#include "util.h"

/* NOTE: Netowrk Address Related Functions */

/**
 * Find last character occurence in the string
 */
static char *proxy_util_strrchr ( const char *s, int c )
{
    size_t i;

    for ( i = strlen ( s ); i--; )
    {
        if ( s[i] == c )
        {
            return ( char * ) ( s + i );
        }
    }

    return NULL;
}

/**
 * Decode ip address and port number
 */
int ip_port_decode ( const char *input, struct sockaddr_storage *saddr )
{
    int port;
    size_t len;
    const char *ptr;
    const char *rptr;
    struct sockaddr_in *saddr_in;
    struct sockaddr_in6 *saddr_in6;
    char straddr[STRADDR_SIZE];

    /* Find first semicolon character */
    if ( !( ptr = strchr ( input, ':' ) ) )
    {
        return -1;
    }

    /* Find last semicolon character */
    if ( !( rptr = proxy_util_strrchr ( input, ':' ) ) )
    {
        return -1;
    }

    /* Clear socket address */
    memset ( saddr, '\0', sizeof ( struct sockaddr_storage ) );

    /* If first semicolon is last semicolon, then IPv4 otherwise IPv6 */
    if ( ptr == rptr )
    {
        /* Prepare socket address */
        saddr_in = ( struct sockaddr_in * ) saddr;
        saddr_in->sin_family = AF_INET;

        /* Validate address string buffer size */
        if ( ( len = ptr - input ) >= sizeof ( straddr ) )
        {
            return -1;
        }

        /* Put address string into the buffer */
        memcpy ( straddr, input, len );
        straddr[len] = '\0';

        /* Parse IP address */
        if ( inet_pton ( AF_INET, straddr, &saddr_in->sin_addr ) <= 0 )
        {
            return -1;
        }

        ptr++;

        /* Parse port number */
        if ( sscanf ( ptr, "%u", &port ) <= 0 || port > 65535 )
        {
            return -1;
        }

        saddr_in->sin_port = htons ( port );

    } else
    {
        /* Prepare socket address */
        saddr_in6 = ( struct sockaddr_in6 * ) saddr;
        saddr_in6->sin6_family = AF_INET6;

        /* Validate address string buffer size */
        if ( ( len = rptr - input ) >= sizeof ( straddr ) )
        {
            return -1;
        }

        /* Skip left bracket */
        if ( len > 0 && *input == '[' )
        {
            input++;
            len--;
        }

        /* Skip right bracked */
        if ( len > 0 )
        {
            if ( input[len - 1] == ']' )
            {
                len--;
            }
        }

        /* Put address string into the buffer */
        memcpy ( straddr, input, len );
        straddr[len] = '\0';

        /* Parse IP address */
        if ( inet_pton ( AF_INET6, straddr, &saddr_in6->sin6_addr ) <= 0 )
        {
            return -1;
        }

        rptr++;

        /* Parse port number */
        if ( sscanf ( rptr, "%u", &port ) <= 0 || port > 65535 )
        {
            return -1;
        }

        saddr_in6->sin6_port = htons ( port );
    }

    return 0;
}

/**
 * Format IP address adn port to string
 */
void format_ip_port ( const struct sockaddr_storage *saddr, char *buffer, size_t size )
{
    int port;
    struct sockaddr_in *saddr_in;
    struct sockaddr_in6 *saddr_in6;
    char straddr[STRADDR_SIZE];

    inet_ntop ( saddr->ss_family, ( const struct sockaddr * ) saddr, straddr,
        sizeof ( struct sockaddr_storage ) );

    switch ( saddr->ss_family )
    {
    case AF_INET:
        saddr_in = ( struct sockaddr_in * ) saddr;
        port = ntohs ( saddr_in->sin_port );
        snprintf ( buffer, size, "%s:%i", straddr, port );
        break;
    case AF_INET6:
        saddr_in6 = ( struct sockaddr_in6 * ) saddr;
        port = ntohs ( saddr_in6->sin6_port );
        snprintf ( buffer, size, "[%s]:%i", straddr, port );
        break;
    default:
        if ( size )
        {
            buffer[0] = '\0';
        }
        break;
    }
}

/* NOTE: Socket Related Functions */

/**
 * Connect remote endpoint asynchronously
 */
int connect_async ( struct proxy_t *proxy, const struct sockaddr_storage *saddr )
{
    int sock;

    /* Create new socket */
    if ( ( sock = socket ( saddr->ss_family, SOCK_STREAM, 0 ) ) < 0 )
    {
        failure ( "cannot create client socket (%i)\n", errno );
        return -2;
    }

    /* Set non-blocking mode on socket */
    if ( socket_set_nonblocking ( proxy, sock ) < 0 )
    {
        shutdown_then_close ( proxy, sock );
        return -1;
    }

    /* Asynchronous connect endpoint */
    if ( connect ( sock, ( const struct sockaddr * ) saddr,
            sizeof ( struct sockaddr_storage ) ) >= 0 )
    {
        failure ( "cannot async-connect endpoint (%i) with socket:%i\n", errno, sock );
        shutdown_then_close ( proxy, sock );
        return -1;
    }

    /* Connecting should be in progress */
    if ( errno != EINPROGRESS )
    {
        failure ( "failed to async-connect endpoint (%i) with socket:%i\n", errno, sock );
        shutdown_then_close ( proxy, sock );
        return -1;
    }

    /* Check for socket error */
    if ( socket_has_error ( sock ) )
    {
        failure ( "encountered an error (%i) on socket:%i\n", errno, sock );
        shutdown_then_close ( proxy, sock );
        return -1;
    }

    verbose ( "async connect pending on socket:%i...\n", sock );

    return sock;
}

/**
 * Bind address to listen socket
 */
int listen_socket ( struct proxy_t *proxy, const struct sockaddr_storage *saddr )
{
    int sock;
    int yes = 1;

    /* Allocate socket */
    if ( ( sock = socket ( saddr->ss_family, SOCK_STREAM, 0 ) ) < 0 )
    {
        failure ( "cannot create listen socket (%i)\n", errno );
        return -1;
    }

    verbose ( "created listen socket\n" );

    /* Allow reusing socket address */
    if ( setsockopt ( sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof ( yes ) ) < 0 )
    {
        failure ( "cannot reuse address (%i) on socket:%i\n", errno, sock );
        shutdown_then_close ( proxy, sock );
        return -1;
    }

    verbose ( "done setting reuse address on socket:%i\n", sock );

    /* Bind socket to address */
    if ( bind ( sock, ( const struct sockaddr * ) saddr, sizeof ( struct sockaddr_storage ) ) < 0 )
    {
        failure ( "cannot bind socket:%i to network address (%i)\n", sock, errno );
        shutdown_then_close ( proxy, sock );
        return -1;
    }

    verbose ( "bound socket:%i to network address\n", sock );

    /* Put socket into listen mode */
    if ( listen ( sock, LISTEN_BACKLOG ) < 0 )
    {
        failure ( "cannot put socket:%i in listen mode (%i)\n", sock, errno );
        shutdown_then_close ( proxy, sock );
        return -1;
    }

    verbose ( "put socket:%i into listen mode\n", sock );

    return sock;
}

/**
 * Check for socket error
 */
int socket_has_error ( int sock )
{
    int so_error = 0;
    socklen_t len = sizeof ( so_error );

    /* Read socket error */
    if ( getsockopt ( sock, SOL_SOCKET, SO_ERROR, &so_error, &len ) < 0 )
    {
        failure ( "cannot read socket:%i error status (%i)\n", sock, errno );
        return -1;
    }

    /* Analyze socket error */
    return !!so_error;
}

/**
 * Set socket non-blocking mode
 */
int socket_set_nonblocking ( struct proxy_t *proxy, int sock )
{
    long mode = 0;

    /* Get current socket mode */
    if ( ( mode = fcntl ( sock, F_GETFL, 0 ) ) < 0 )
    {
        failure ( "cannot get socket:%i mode (%i)\n", sock, errno );
        return -1;
    }

    /* Update socket mode */
    if ( fcntl ( sock, F_SETFL, mode | O_NONBLOCK ) < 0 )
    {
        failure ( "cannot set socket:%i mode (%i)\n", sock, errno );
        return -1;
    }

    verbose ( "set non-blocking mode on socket:%i\n", sock );

    return 0;
}

/**
 * Forward data between sockets
 */
int socket_forward_data ( struct proxy_t *proxy, int srcfd, int dstfd )
{
    int len = FORWARD_CHUNK_LEN;
    int recvlim;
    int sendlim;
    int sendwip;
    socklen_t optlen;
    uint8_t buffer[FORWARD_CHUNK_LEN];

    UNUSED ( proxy );

    if ( ioctl ( srcfd, FIONREAD, &recvlim ) < 0 )
    {
        failure ( "cannot get socket:%i available bytes count (%i)\n", srcfd, errno );
        return -1;
    }

    if ( !recvlim )
    {
        verbose ( "lost connection on socket:%i\n", srcfd );
        return -1;
    }

    verbose ( "socket:%i available bytes count: %i\n", srcfd, recvlim );

    if ( recvlim < len )
    {
        len = recvlim;
        verbose ( "bytes count limited to buffer size: %i\n", len );
    }

    if ( ioctl ( dstfd, TIOCOUTQ, &sendwip ) < 0 )
    {
        failure ( "cannot get socket:%i pending bytes count (%i)\n", dstfd, errno );
        return -1;
    }

    verbose ( "socket:%i pending bytes count: %i\n", dstfd, sendwip );

    optlen = sizeof ( sendlim );

    if ( getsockopt ( dstfd, SOL_SOCKET, SO_SNDBUF, &sendlim, &optlen ) < 0 )
    {
        failure ( "cannot get socket:%i output capacity (%i)\n", dstfd, errno );
        return -1;
    }

    if ( optlen != sizeof ( sendlim ) )
    {
        failure ( "socket:%i output capacity data type is invalid\n", dstfd );
        return -1;
    }

    verbose ( "socket:%i output capacity: %i\n", dstfd, sendlim );

    if ( sendwip > sendlim )
    {
        failure ( "socket:%i capacity is less than data pending\n", dstfd );
        return -1;
    }

    sendlim -= sendwip;

    if ( !sendlim )
    {
        failure ( "socket:%i was expected to be write ready\n", dstfd );
        return -1;
    }

    if ( sendlim < len )
    {
        len = sendlim;
        verbose ( "bytes count limited to socket:%i output capacity: %i\n", dstfd, len );
    }

    if ( !len )
    {
        verbose ( "socket:%i no data to be transfered into\n", dstfd );
        return -1;
    }

    if ( recv ( srcfd, buffer, len, MSG_PEEK ) < len )
    {
        failure ( "cannot receive data from socket:%i\n", srcfd );
        return -1;
    }

    if ( ( len = send ( dstfd, buffer, len, MSG_NOSIGNAL ) ) < 0 )
    {
        failure ( "cannot send data to socket:%i\n", dstfd );
        return -1;
    }

    if ( recv ( srcfd, buffer, len, 0 ) < len )
    {
        failure ( "cannot skip data from socket:%i\n", srcfd );
        return -1;
    }

    verbose ( "forwarded %i byte(s) from socket:%i to socket:%i\n", len, srcfd, dstfd );

    return len;
}

/**
 * Shutdown and close the socket
 */
void shutdown_then_close ( struct proxy_t *proxy, int sock )
{
    shutdown ( sock, SHUT_RDWR );
    verbose ( "socket:%i has been shutdown\n", sock );
    close ( sock );
    verbose ( "socket:%i has been closed\n", sock );
}

/* NOTE: Data Queue Related Functions */

/**
 * Reset data queue content
 */
void queue_reset ( struct queue_t *queue )
{
    queue->len = 0;
}

/**
 * Push bytes into data queue
 */
int queue_push ( struct queue_t *queue, const uint8_t * bytes, size_t len )
{
    if ( queue->len + len > sizeof ( queue->arr ) )
    {
        return -1;
    }

    memcpy ( queue->arr + queue->len, bytes, len );
    queue->len += len;
    return 0;
}

/**
 * Set data queue content
 */
int queue_set ( struct queue_t *queue, const uint8_t * bytes, size_t len )
{
    queue_reset ( queue );
    return queue_push ( queue, bytes, len );
}

/**
 * Shift bytes from data queue
 */
int queue_shift ( struct queue_t *queue, int fd )
{
    size_t i;
    ssize_t len;

    if ( ( len = send ( fd, queue->arr, queue->len, MSG_NOSIGNAL ) ) < 0 )
    {
        return -1;
    }

    queue->len -= len;

    for ( i = 0; i < queue->len; i++ )
    {
        queue->arr[i] = queue->arr[len + i];
    }

    return 0;
}

/**
 * Assert minimum data length
 */
int check_enough_data ( struct proxy_t *proxy, struct stream_t *stream, size_t value )
{
    if ( stream->queue.len < value )
    {
        verbose ( "awaiting more bytes (%lu/%lu) from socket:%i...\n",
            ( unsigned long ) stream->queue.len, ( unsigned long ) value, stream->fd );
        return -1;
    }

    return 0;
}

/* NOTE: Event Listenning Related Functions */

/**
 * Setup proxy events listenning
 */
int proxy_events_setup ( struct proxy_t *proxy )
{
    /* Create epoll fd if possible */
    if ( ( proxy->epoll_fd = epoll_create ( 0 ) ) >= 0 )
    {
        verbose ( "epoll initialized\n" );

    } else
    {
        if ( ( proxy->epoll_fd = epoll_create1 ( 0 ) ) >= 0 )
        {
            verbose ( "epoll-1 initialized\n" );

        } else
        {
            verbose ( "epoll not supported\n" );
        }
    }

    return 0;
}

/**
 * Build stream event list with poll
 */
int build_poll_list ( struct proxy_t *proxy, struct pollfd *poll_list, size_t *poll_len )
{
    size_t poll_size;
    size_t poll_rlen = 0;
    struct stream_t *iter;
    struct pollfd *pollref;

    poll_size = *poll_len;

    /* Reset poll references */
    for ( iter = proxy->stream_head; iter; iter = iter->next )
    {
        iter->pollref = NULL;
    }

    /* Append file descriptors to the poll list */
    for ( iter = proxy->stream_head; iter; iter = iter->next )
    {
        /* Assert poll list index */
        if ( poll_rlen >= poll_size )
        {
            failure ( "poll list capacity exceeded\n" );
            return -1;
        }

        /* Add stream to poll list if applicable */
        if ( iter->events )
        {
            pollref = poll_list + poll_rlen;
            pollref->fd = iter->fd;
            pollref->events = POLLERR | POLLHUP | iter->events;
            iter->pollref = pollref;
            poll_rlen++;
            verbose ( "poll list push socket:%i with events: %s%s%s%s\n", pollref->fd,
                POLL_EVENTS_TO_4xSTR ( pollref->events ) );
        }
    }

    verbose ( "poll list length is %lu event(s)\n", ( unsigned long ) poll_rlen );
    *poll_len = poll_rlen;

    return 0;
}

/**
 * Update streams revents with poll
 */
void update_revents_poll ( struct proxy_t *proxy )
{
    struct stream_t *iter;

    for ( iter = proxy->stream_head; iter; iter = iter->next )
    {
        iter->revents = iter->pollref ? iter->pollref->revents : 0;
        if ( proxy->verbose )
        {
            if ( iter->revents )
            {
                verbose ( "events returned for socket:%i: %s%s%s%s\n", iter->fd,
                    POLL_EVENTS_TO_4xSTR ( iter->revents ) );
            }
        }
    }
}

/**
 * Watch stream events with poll
 */
int watch_streams_poll ( struct proxy_t *proxy )
{
    int nfds;
    size_t poll_len;
    struct pollfd poll_list[POOL_SIZE];

    /* Set poll list size */
    poll_len = sizeof ( poll_list ) / sizeof ( struct pollfd );

    /* Rebuild poll event list */
    if ( build_poll_list ( proxy, poll_list, &poll_len ) < 0 )
    {
        failure ( "building poll list failed (%i)\n", errno );
        return -1;
    }

    verbose ( "waiting for events with poll...\n" );

    /* Poll events */
    if ( ( nfds = poll ( poll_list, poll_len, POLL_TIMEOUT_MSEC ) ) < 0 )
    {
        failure ( "poll events failed (%i)\n", errno );
        return -1;
    }

    /* Update stream poll revents */
    update_revents_poll ( proxy );

    return nfds;
}

/**
 * Convert poll to epoll events
 */
int poll_to_epoll_events ( int poll_events )
{
    int epoll_events = 0;

    if ( poll_events & POLLERR )
    {
        epoll_events |= EPOLLERR;
    }

    if ( poll_events & POLLHUP )
    {
        epoll_events |= EPOLLHUP;
    }

    if ( poll_events & POLLIN )
    {
        epoll_events |= EPOLLIN;
    }

    if ( poll_events & POLLOUT )
    {
        epoll_events |= EPOLLOUT;
    }

    return epoll_events;
}

/**
 * Convert epoll to poll events
 */
int epoll_to_poll_events ( int epoll_events )
{
    int poll_events = 0;

    if ( epoll_events & EPOLLERR )
    {
        poll_events |= POLLERR;
    }

    if ( epoll_events & EPOLLHUP )
    {
        poll_events |= POLLHUP;
    }

    if ( epoll_events & EPOLLIN )
    {
        poll_events |= POLLIN;
    }

    if ( epoll_events & EPOLLOUT )
    {
        poll_events |= POLLOUT;
    }

    return epoll_events;
}

/**
 * Build stream event list with epoll
 */
int build_epoll_list ( struct proxy_t *proxy )
{
    int operation;
    struct stream_t *iter;
    struct epoll_event event;

    /* Append file descriptors to the poll list */
    for ( iter = proxy->stream_head; iter; iter = iter->next )
    {
        if ( iter->events )
        {
            if ( !iter->pollref || iter->events != iter->levents )
            {
                event.data.ptr = iter;
                event.events = poll_to_epoll_events ( iter->events | POLLERR | POLLHUP );
                operation = iter->pollref ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

                if ( epoll_ctl ( proxy->epoll_fd, operation, iter->fd, &event ) < 0 )
                {
                    failure ( "epoll list cannot update socket:%i with events 0x%.2x\n", iter->fd,
                        event.events );
                    return -1;
                }

                verbose ( "epoll list updated socket:%i with events: %s%s%s%s\n", iter->fd,
                    EPOLL_EVENTS_TO_4xSTR ( event.events ) );

                iter->levents = iter->events;
                iter->pollref = EPOLLREF;
            }

        } else if ( iter->pollref )
        {
            verbose ( "epoll list removed socket:%i\n", proxy->epoll_fd );

            if ( epoll_ctl ( proxy->epoll_fd, EPOLL_CTL_DEL, iter->fd, NULL ) < 0 )
            {
                return -1;
            }

            iter->pollref = NULL;
        }
    }

    return 0;
}

/**
 * Update streams revents with poll
 */
void update_revents_epoll ( struct proxy_t *proxy, int nfds, struct epoll_event *events )
{
    int i;
    struct stream_t *stream;

    for ( stream = proxy->stream_head; stream; stream = stream->next )
    {
        stream->revents = 0;
    }

    for ( i = 0; i < nfds; i++ )
    {
        if ( ( stream = events[i].data.ptr ) )
        {
            stream->revents = epoll_to_poll_events ( events[i].events );

            if ( proxy->verbose )
            {
                if ( stream->revents )
                {
                    verbose ( "events returned for socket:%i with events: %s%s%s%s\n", stream->fd,
                        POLL_EVENTS_TO_4xSTR ( stream->revents ) );
                }
            }
        }
    }
}

/**
 * Watch stream events with epoll
 */
int watch_streams_epoll ( struct proxy_t *proxy )
{
    int nfds;
    struct epoll_event events[POOL_SIZE];

    /* Rebuild epoll event list */
    if ( build_epoll_list ( proxy ) < 0 )
    {
        failure ( "building epoll list failed (%i)\n", errno );
        return -1;
    }

    verbose ( "waiting for events with epoll...\n" );

    /* E-Poll events */
    if ( ( nfds = epoll_wait ( proxy->epoll_fd, events, POOL_SIZE, POLL_TIMEOUT_MSEC ) ) < 0 )
    {
        failure ( "epoll wait failed (%i)\n", errno );
        return -1;
    }

    /* Update stream epoll revents */
    update_revents_epoll ( proxy, nfds, events );

    return nfds;
}

/**
 * Watch stream events
 */
int watch_streams ( struct proxy_t *proxy )
{
    if ( proxy->epoll_fd >= 0 )
    {
        return watch_streams_epoll ( proxy );
    }

    return watch_streams_poll ( proxy );
}

/* NOTE: Stream Related Functions */

/**
 * Insert new stream structure into the list
 */
struct stream_t *insert_stream ( struct proxy_t *proxy, int sock )
{
    struct stream_t *ptr;
    struct stream_t *lim;
    struct stream_t *stream = NULL;

    ptr = proxy->stream_pool;
    lim =
        ( struct stream_t * ) ( ( ( uint8_t * ) proxy->stream_pool ) +
        POOL_SIZE * proxy->stream_size );

    while ( ptr < lim )
    {
        if ( !ptr->allocated )
        {
            stream = ptr;
            break;
        }
        ptr = ( struct stream_t * ) ( ( ( uint8_t * ) ptr ) + proxy->stream_size );
    }

    if ( !stream )
    {
        failure ( "stream pool is full\n" );
        return NULL;
    }

    memset ( stream, '\0', proxy->stream_size );
    stream->role = S_INVALID;
    stream->fd = sock;
    stream->level = LEVEL_NONE;
    stream->allocated = 1;
    stream->next = proxy->stream_head;

    if ( proxy->stream_head )
    {
        proxy->stream_head->prev = stream;

    } else
    {
        proxy->stream_tail = stream;
    }

    proxy->stream_head = stream;

    verbose ( "created new stream with socket:%i\n", sock );

    return stream;
}

/**
 * Accept a new stream
 */
struct stream_t *accept_new_stream ( struct proxy_t *proxy, int lfd )
{
    int sock;
    struct stream_t *stream;

    /* Accept incoming connection */
    if ( ( sock = accept ( lfd, NULL, NULL ) ) < 0 )
    {
        failure ( "cannot accept incoming connection (%i) on socket:%i\n", errno, lfd );
        return NULL;
    }

    /* Set non-blocking mode on socket */
    if ( socket_set_nonblocking ( proxy, sock ) < 0 )
    {
        shutdown_then_close ( proxy, sock );
        return NULL;
    }

    /* Try allocating new stream */
    if ( !( stream = insert_stream ( proxy, sock ) ) )
    {
        verbose ( "stream pool is full, need to force cleanup...\n" );
        force_cleanup ( proxy, NULL );
        stream = insert_stream ( proxy, sock );
    }

    /* Check if stream was allocated */
    if ( !stream )
    {
        shutdown_then_close ( proxy, sock );
        return NULL;
    }

    return stream;
}

/**
 * Handle stream data forward
 */
int handle_forward_data ( struct proxy_t *proxy, struct stream_t *stream )
{
    if ( !stream->neighbour || stream->level != LEVEL_FORWARDING )
    {
        return -1;
    }

    if ( stream->revents & POLLOUT )
    {
        if ( socket_forward_data ( proxy, stream->neighbour->fd, stream->fd ) < 0 )
        {
            return -1;
        }

        stream->events &= ~POLLOUT;
        stream->neighbour->events |= POLLIN;

    } else if ( stream->revents & POLLIN )
    {
        stream->events &= ~POLLIN;
        stream->neighbour->events |= POLLOUT;
    }

    return 0;
}

/**
 * Show relations statistics
 */
void show_stats ( struct proxy_t *proxy )
{
    int a_forwarding = 0;
    int b_forwarding = 0;
    int a_total = 0;
    int b_total = 0;
    int total = 0;
    struct stream_t *iter;

    for ( iter = proxy->stream_head; iter; iter = iter->next )
    {
        if ( iter->role == S_PORT_A )
        {
            if ( iter->level == LEVEL_FORWARDING )
            {
                a_forwarding++;
            }
            a_total++;

        } else if ( iter->role == S_PORT_B )
        {
            if ( iter->level == LEVEL_FORWARDING )
            {
                b_forwarding++;
            }
            b_total++;
        }

        total++;
    }

    info ( "load: A:%i/%i B:%i/%i *:%i/%i\n", a_forwarding, a_total, b_forwarding,
        b_total, total, POOL_SIZE );
}

/**
 * Free and remove a single stream from the list
 */
void remove_stream ( struct proxy_t *proxy, struct stream_t *stream )
{
    if ( stream->fd >= 0 )
    {
        if ( stream->pollref )
        {
            epoll_ctl ( proxy->epoll_fd, EPOLL_CTL_DEL, stream->fd, NULL );
        }

        shutdown_then_close ( proxy, stream->fd );
        stream->fd = -1;
    }

    if ( stream == proxy->stream_head )
    {
        proxy->stream_head = stream->next;
    }

    if ( stream == proxy->stream_tail )
    {
        proxy->stream_tail = stream->prev;
    }

    if ( stream->next )
    {
        stream->next->prev = stream->prev;
    }

    if ( stream->prev )
    {
        stream->prev->next = stream->next;
    }

    stream->allocated = 0;
}

/*
 * Abandon associated pair of streams
 */
void remove_relation ( struct stream_t *stream )
{
    if ( stream->neighbour )
    {
        stream->neighbour->abandoned = 1;
    }
    stream->abandoned = 1;
}

/**
 * Remove all relations
 */
void remove_all_streams ( struct proxy_t *proxy )
{
    struct stream_t *iter;
    struct stream_t *next;

    verbose ( "removing all streams...\n" );

    for ( iter = proxy->stream_head; iter; iter = next )
    {
        next = iter->next;
        remove_stream ( proxy, iter );
        iter = next;
    }
}

/**
 * Remove pending streams
 */
void remove_pending_streams ( struct proxy_t *proxy )
{
    struct stream_t *iter;

    for ( iter = proxy->stream_head; iter; iter = iter->next )
    {
        if ( ( iter->role == S_PORT_A || iter->role == S_PORT_B )
            && iter->level != LEVEL_FORWARDING )
        {
            verbose ( "cleaning up pending stream with socket:%i...\n", iter->fd );
            remove_relation ( iter );
        }
    }
}

/**
 * Remove abandoned streams
 */
void cleanup_streams ( struct proxy_t *proxy )
{
    struct stream_t *iter;
    struct stream_t *next;

    for ( iter = proxy->stream_head; iter; iter = next )
    {
        next = iter->next;

        if ( iter->abandoned )
        {
            remove_stream ( proxy, iter );
        }
    }
}

/**
 * Remove oldest forwarding relation
 */
void force_cleanup ( struct proxy_t *proxy, const struct stream_t *excl )
{
    struct stream_t *iter;

    for ( iter = proxy->stream_tail; iter; iter = iter->prev )
    {
        if ( iter != excl && iter->abandoned )
        {
            verbose ( "will remove an abandoned stream with socket:%i...\n", iter->fd );
            remove_relation ( iter );
            remove_stream ( proxy, iter );
            return;
        }
    }

    for ( iter = proxy->stream_tail; iter; iter = iter->prev )
    {
        if ( iter != excl && ( iter->role == S_PORT_A || iter->role == S_PORT_B ) )
        {
            verbose ( "need to get rid of stream with socket:%i...\n", iter->fd );
            remove_relation ( iter );
            remove_stream ( proxy, iter );
            return;
        }
    }
}

/**
 * Stream event handling cycle
 */
int handle_streams_cycle ( struct proxy_t *proxy )
{
    int status;
    struct stream_t *iter;
    struct stream_t *next;

    /* Cleanup streams */
    cleanup_streams ( proxy );

    /* Watch streams events */
    if ( ( status = watch_streams ( proxy ) ) < 0 )
    {
        failure ( "failed to watch events (%i)\n", errno );
        return -1;
    }

    /* Do some cleanup */
    if ( !status )
    {
        remove_pending_streams ( proxy );
        cleanup_streams ( proxy );
        show_stats ( proxy );
        return 0;
    }

    /* Process stream list */
    for ( iter = proxy->stream_head; iter; iter = next )
    {
        next = iter->next;

        if ( !iter->abandoned && iter->revents )
        {
            if ( iter->revents & ( POLLERR | POLLHUP ) )
            {
                verbose ( "stream with socket:%i got POLLERR/POLLHUP...\n", iter->fd );
                remove_relation ( iter );

            } else
            {
                if ( handle_stream_events ( proxy, iter ) < 0 )
                {
                    return -1;
                }
            }
        }
    }

    return 0;
}
