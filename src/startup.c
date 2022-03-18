/* ------------------------------------------------------------------
 * V-Socks - Main Program File
 * ------------------------------------------------------------------ */

#include "proxy.h"

/**
 * Show program usage message
 */
static void show_usage ( void )
{
    V ( printf
        ( "[vsck] usage: vsocks listen-addr:listen-port socks5-addr:socks5s-port [[+]dest-addr:dest-port]\n"
            "\n" "       listen-addr       Gateway address\n"
            "       listen-port       Gateway port\n"
            "       socks5-addr       Socks server address\n"
            "       socks5-port       Socks-5 server port\n"
            "       dest-addr         Destination address\n"
            "       dest-port         Destination port\n\n" ) );
}

/**
 * Decode ip address and port number
 */
static int ip_port_decode ( const char *input, unsigned int *addr, unsigned short *port )
{
    unsigned int lport;
    size_t len;
    const char *ptr;
    char buffer[32];

    /* Find port number separator */
    if ( !( ptr = strchr ( input, ':' ) ) )
    {
        return -1;
    }

    /* Validate destination buffer size */
    if ( ( len = ptr - input ) >= sizeof ( buffer ) )
    {
        return -1;
    }

    /* Save address string */
    memcpy ( buffer, input, len );
    buffer[len] = '\0';

    /* Parse IP address */
    if ( inet_pton ( AF_INET, buffer, addr ) <= 0 )
    {
        return -1;
    }

    ptr++;

    /* Parse port b number */
    if ( sscanf ( ptr, "%u", &lport ) <= 0 || lport > 65535 )
    {
        return -1;
    }

    *port = lport;
    return 0;
}

/**
 * Program entry point
 */
int main ( int argc, char *argv[] )
{
    struct proxy_t proxy;
    const char *dest_addr_port;

    /* Show program version */
    V ( printf ( "[vsck] VSocks - ver. " VSOCKS_VERSION "\n" ) );

    /* Validate arguments count */
    if ( argc < 3 )
    {
        show_usage (  );
        return 1;
    }

    memset ( &proxy, '\0', sizeof ( proxy ) );

    if ( ip_port_decode ( argv[1], &proxy.listen_addr, &proxy.listen_port ) < 0 )
    {
        show_usage (  );
        return 1;
    }

    if ( ip_port_decode ( argv[2], &proxy.socks5_addr, &proxy.socks5_port ) < 0 )
    {
        show_usage (  );
        return 1;
    }

    if ( argc > 3 )
    {
        dest_addr_port = argv[3];

        if ( dest_addr_port[0] == '+' )
        {
            proxy.use_second_handshake = 1;
            dest_addr_port++;
        }

        if ( ip_port_decode ( dest_addr_port, &proxy.dest_addr, &proxy.dest_port ) < 0 )
        {
            show_usage (  );
            return 1;
        }
    }
#ifndef VERBOSE_MODE
    if ( daemon ( 0, 0 ) < 0 )
    {
        return -1;
    }
#endif

    for ( ;; )
    {
        if ( proxy_task ( &proxy ) < 0 )
        {
            if ( errno == EINTR || errno == ENOTCONN )
            {
                V ( printf ( "[vsck] retrying in 1 sec...\n" ) );
                sleep ( 1 );

            } else
            {
                V ( printf ( "[vsck] exit status: %i\n", errno ) );
                return 1;
            }
        }
    }

    V ( printf ( "[vsck] exit status: success\n" ) );
    return 0;
}
