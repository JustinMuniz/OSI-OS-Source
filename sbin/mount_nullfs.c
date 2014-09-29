#ifndef lint
static const char copyright[] = "@(#) Copyright (c) 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif 
#ifndef lint
#if 0
static char sccsid[] = "@(#)mount_null.c	8.6 (Berkeley) 4/26/95";
#endif
static const char rcsid[] = "$FreeBSD$";
#endif 
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include "mntopts.h"
int subdir( const char *, const char * );
static void usage( void )
__dead2;
int main( int argc, char *argv[] ) {
	struct iovec *iov;
	char *p, *val;
	char source[MAXPATHLEN];
	char target[MAXPATHLEN];
	char errmsg[255];
	int ch, iovlen;
	char nullfs[] = "nullfs";
	iov = NULL;
	iovlen = 0;
	errmsg[0] = '\0';
	while ( ( ch = getopt( argc, argv, "o:" ) ) != -1 )
		switch ( ch ) {
			case 'o':
				val = strdup( "" );
				p = strchr( optarg, '=' );
				if ( p != NULL ) {
					free( val );
					*p = '\0';
					val = p + 1;
				}
				build_iovec( &iov, &iovlen, optarg, val, ( size_t ) - 1 );
				break;
			case '?':
			default:
				usage();
		}
	argc -= optind;
	argv += optind;
	if ( argc != 2 ) usage();
	if ( checkpath( argv[0], target ) != 0 ) err( EX_USAGE, "%s", target );
	if ( checkpath( argv[1], source ) != 0 ) err( EX_USAGE, "%s", source );
	if ( subdir( target, source ) || subdir( source, target ) ) errx( EX_USAGE, "%s (%s) and %s are not distinct paths", argv[0], target, argv[1] );
	build_iovec( &iov, &iovlen, "fstype", nullfs, ( size_t ) - 1 );
	build_iovec( &iov, &iovlen, "fspath", source, ( size_t ) - 1 );
	build_iovec( &iov, &iovlen, "target", target, ( size_t ) - 1 );
	build_iovec( &iov, &iovlen, "errmsg", errmsg, sizeof( errmsg ) );
	if ( nmount( iov, iovlen, 0 ) < 0 ) {
		if ( errmsg[0] != 0 ) err( 1, "%s: %s", source, errmsg );
		else err( 1, "%s", source );
	}
	exit( 0 );
}
int subdir( const char *p, const char *dir ) {
	int l;
	l = strlen( dir );
	if ( l <= 1 ) return ( 1 );
	if ( ( strncmp( p, dir, l ) == 0 ) && ( p[l] == '/' || p[l] == '\0' ) ) return ( 1 );
	return ( 0 );
}
static void usage( void ) {
	(void) fprintf( stderr, "usage: mount_nullfs [-o options] target mount-point\n" );
	exit( 1 );
}
