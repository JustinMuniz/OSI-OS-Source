#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static void usage( void )
__dead2;
int main( int argc, char *argv[] ) {
	char buf[PATH_MAX];
	char *p;
	const char *path;
	int ch, qflag, rval;
	qflag = 0;
	while ( ( ch = getopt( argc, argv, "q" ) ) != -1 ) {
		switch ( ch ) {
			case 'q':
				qflag = 1;
				break;
			case '?':
			default:
				usage();
		}
	}
	argc -= optind;
	argv += optind;
	path = *argv != NULL ? *argv++ : ".";
	rval = 0;
	do {
		if ( ( p = realpath( path, buf ) ) == NULL ) {
			if ( !qflag ) warn( "%s", path );
			rval = 1;
		} else (void) printf( "%s\n", p );
	} while ( ( path = *argv++ ) != NULL );
	exit( rval );
}
static void usage( void ) {
	(void) fprintf( stderr, "usage: realpath [-q] [path ...]\n" );
	exit( 1 );
}
