#if 0
#ifndef lint
static char const copyright[] =
"@(#) Copyright (c) 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif 
#ifndef lint
static char sccsid[] = "@(#)rmdir.c	8.3 (Berkeley) 4/2/94";
#endif 
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int rm_path( char * );
static void usage( void );
static int pflag;
static int vflag;
int main( int argc, char *argv[] ) {
	int ch, errors;
	while ( ( ch = getopt( argc, argv, "pv" ) ) != -1 )
		switch ( ch ) {
			case 'p':
				pflag = 1;
				break;
			case 'v':
				vflag = 1;
				break;
			case '?':
			default:
				usage();
		}
	argc -= optind;
	argv += optind;
	if ( argc == 0 ) usage();
	for ( errors = 0; *argv; argv++ ) {
		if ( rmdir( *argv ) < 0 ) {
			warn( "%s", *argv );
			errors = 1;
		} else {
			if ( vflag ) printf( "%s\n", *argv );
			if ( pflag ) errors |= rm_path( *argv );
		}
	}
	exit( errors );
}
static int rm_path( char *path ) {
	char *p;
	p = path + strlen( path );
	while ( --p > path && *p == '/' )
		;
	*++p = '\0';
	while ( ( p = strrchr( path, '/' ) ) != NULL ) {
		while ( --p >= path && *p == '/' )
			;
		*++p = '\0';
		if ( p == path ) break;
		if ( rmdir( path ) < 0 ) {
			warn( "%s", path );
			return ( 1 );
		}
		if ( vflag ) printf( "%s\n", path );
	}
	return ( 0 );
}
static void usage( void ) {
	(void) fprintf( stderr, "usage: rmdir [-pv] directory ...\n" );
	exit( 1 );
}
