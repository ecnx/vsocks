/* ------------------------------------------------------------------
 * V-Socks - Main Program File
 * ------------------------------------------------------------------ */

#include "vsocks.h"

/**
 * Show program usage message
 */
static void show_usage ( void )
{
    failure ( "usage: vsocks [-vd] listen-addr:listen-port socks5-addr:socks5s-port\n\n"
        "       option -v         Enable verbose logging\n"
        "       option -d         Run in background\n"
        "       listen-addr       Gateway address\n"
        "       listen-port       Gateway port\n"
        "       socks5-addr       Socks server address\n"
        "       socks5-port       Socks-5 server port\n\n"
        "Note: Both IPv4 and IPv6 can be used\n\n" );
}

/**
 * Program entry point
 */
int main ( int argc, char *argv[] )
{
    int arg_off = 0;
    int daemon_flag = 0;
    struct proxy_t proxy = { 0 };

    /* Show program version */
    info ( "[vsck] VSocks - ver. " VSOCKS_VERSION "\n" );

    /* Validate arguments count */
    if ( argc < 3 )
    {
        show_usage (  );
        return 1;
    }

    /* Check for options */
    if ( argv[1][0] == '-' )
    {
        arg_off = 1;
        proxy.verbose = !!strchr ( argv[1], 'v' );
        daemon_flag = !!strchr ( argv[1], 'd' );
    }

    /* Re-validate arguments count */
    if ( argc < arg_off + 3 )
    {
        show_usage (  );
        return 1;
    }

    /* Parse listen address and port */
    if ( ip_port_decode ( argv[arg_off + 1], &proxy.entrance ) < 0 )
    {
        show_usage (  );
        return 1;
    }

    /* Parse proxy address and port */
    if ( ip_port_decode ( argv[arg_off + 2], &proxy.socks5 ) < 0 )
    {
        show_usage (  );
        return 1;
    }

    /* Run in background if needed */
    if ( daemon_flag )
    {
        if ( daemon ( 0, 0 ) < 0 )
        {
            failure ( "cannot run in background (%i)\n", errno );
            return 1;
        }
    }

    /* Launch the proxy task */
    if ( proxy_task ( &proxy ) < 0 )
    {
        failure ( "exit status: %i\n", errno );
        return 1;
    }

    info ( "exit status: success\n" );
    return 0;
}
