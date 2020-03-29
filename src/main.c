/* ------------------------------------------------------------------
 * Ella Proxy - Main Program File
 * ------------------------------------------------------------------ */

#include "ella.h"

/**
 * Show Ella Proxy program usage message
 */
static void show_usage ( void )
{
    N ( printf ( "[ella] usage: vsocks l-addr l-port s-addr s-port\n"
            "\n"
            "       l-addr       Listen address\n"
            "       l-port       Listen port\n"
            "       s-addr       Socks address\n" "       s-port       Socks port\n" "\n" ) );
}

/**
 * Program entry point
 */
int main ( int argc, char *argv[] )
{
    unsigned int port = 0;
    struct ella_params_t params;

    /* Show program version */
    N ( printf ( "[ella] Ella Proxy - ver. " ELLA_VERSION " [vsocks]\n" ) );

    /* Validate arguments count */
    if ( argc < 5 )
    {
        show_usage (  );
        return 1;
    }
#ifdef SILENT_MODE
    daemon ( 0, 0 );
#endif

    if ( inet_pton ( AF_INET, argv[1], &params.listen_addr ) <= 0 )
    {
        show_usage (  );
        return 1;
    }

    if ( sscanf ( argv[2], "%u", &port ) <= 0 || port > 65535 )
    {
        show_usage (  );
        return 1;
    }

    params.listen_port = port;

    if ( inet_pton ( AF_INET, argv[3], &params.socks_addr ) <= 0 )
    {
        show_usage (  );
        return 1;
    }

    if ( sscanf ( argv[4], "%u", &port ) <= 0 || port > 65535 )
    {
        show_usage (  );
        return 1;
    }

    params.socks_port = port;

    if ( proxy_task ( &params ) < 0 )
    {
        N ( printf ( "[ella] exit status: %i\n", errno ) );
        return 1;
    }

    N ( printf ( "[ella] exit status: success\n" ) );
    return 0;
}
