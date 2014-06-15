#if 0
#ifndef lint
static char const copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif 
#ifndef lint
static char sccsid[] = "@(#)hostname.c	8.1 (Berkeley) 5/31/93";
#endif 
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void usage( void )
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
			case '?':
			default:
				usage();
		}
	argc -= optind;
	argv += optind;
	if ( argc > 1 ) usage();
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
static void usage( void ) {
	(void) fprintf( stderr, "usage: hostname [-fs] [name-of-host]\n" );
	exit( 1 );
}
