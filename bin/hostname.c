#include <sys/cdefs.h>
#include <sys/param.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
__dead2;
int main( int argc, char *argv[] ) {
	int ch, sflag;
	char *p, hostname[MAXHOSTNAMELEN];
	sflag = 0;
	while ( ( ch = getopt( argc, argv, "fs" ) ) != -1 )
		switch ( ch ) {
			case 'f':
				break;
			case 's':
				sflag = 1;
				break;
			default:
				exit( 1 );
		}
	argc -= optind;
	argv += optind;
	if ( argc > 1 ) exit( 1 );
	if ( *argv ) {
		if ( sethostname( *argv, (int) strlen( *argv ) ) ) err( 1, "sethostname" );
	} else {
		if ( gethostname( hostname, (int) sizeof( hostname ) ) ) err( 1, "gethostname" );
		if ( sflag ) {
			p = strchr( hostname, '.' );
			if ( p != NULL ) *p = '\0';
		}
		(void) printf( "%s\n", hostname );
	}
	exit( 0 );
}
