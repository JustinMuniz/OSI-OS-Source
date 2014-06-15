#if 0
#ifndef lint
static char const copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif 
#ifndef lint
static char const sccsid[] = "From: @(#)hostname.c	8.1 (Berkeley) 5/31/93";
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
static void usage( void );
int main( int argc, char *argv[] ) {
	int ch;
	char domainname[MAXHOSTNAMELEN];
	while ( ( ch = getopt( argc, argv, "" ) ) != -1 )
		switch ( ch ) {
			case '?':
			default:
				usage();
		}
	argc -= optind;
	argv += optind;
	if ( argc > 1 ) usage();
	if ( *argv ) {
		if ( setdomainname( *argv, (int) strlen( *argv ) ) ) err( 1, "setdomainname" );
	} else {
		if ( getdomainname( domainname, (int) sizeof( domainname ) ) ) err( 1, "getdomainname" );
		(void) printf( "%s\n", domainname );
	}
	exit( 0 );
}
static void usage( void ) {
	(void) fprintf( stderr, "usage: domainname [ypdomain]\n" );
	exit( 1 );
}
