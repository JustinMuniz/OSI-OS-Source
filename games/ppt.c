#ifndef lint
static const char copyright[] = "@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif 
#ifndef lint
#if 0
static char sccsid[] = "@(#)ppt.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] = "$FreeBSD$";
#endif 
#include <stdio.h>
#include <stdlib.h>
static void putppt( int );
int main( int argc, char **argv ) {
	int c;
	char *p;
	(void) puts( "___________" );
	if ( argc > 1 ) while ( ( p = *++argv ) )
		for ( ; *p; ++p )
			putppt( (int) *p );
	else while ( ( c = getchar() ) != EOF )
		putppt( c );
	(void) puts( "___________" );
	exit( 0 );
}
static void putppt( int c ) {
	int i;
	(void) putchar( '|' );
	for ( i = 7; i >= 0; i-- ) {
		if ( i == 2 ) (void) putchar( '.' );
		if ( ( c & ( 1 << i ) ) != 0 ) (void) putchar( 'o' );
		else (void) putchar( ' ' );
	}
	(void) putchar( '|' );
	(void) putchar( '\n' );
}
