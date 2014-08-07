#include <sys/cdefs.h>
#include <sys/param.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main( int argc, char *argv[] ) {
	int ch;
	char domainname[MAXHOSTNAMELEN];
	while ( ( ch = getopt( argc, argv, "" ) ) != -1 )
		exit( 1 );
	argc -= optind;
	argv += optind;
	if ( argc > 1 ) exit( 1 );
	if ( *argv )
		if ( setdomainname( *argv, (int) strlen( *argv ) ) ) err( 1, "setdomainname" );
	else if ( getdomainname( domainname, (int) sizeof( domainname ) ) ) err( 1, "getdomainname" );
		(void) printf( "%s\n", domainname );
	exit( 0 );
}
