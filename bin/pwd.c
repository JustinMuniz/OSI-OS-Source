#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static char *getcwd_logical( void );
int main( int argc, char *argv[] ) {
	int physical;
	int ch;
	char *p;
	physical = 1;
	while ( ( ch = getopt( argc, argv, "LP" ) ) != -1 )
		switch ( ch ) {
			case 'L':
				physical = 0;
				break;
			case 'P':
				physical = 1;
				break;
			case '?':
			default:
				exit( 1 );
		}
	argc -= optind;
	argv += optind;
	if ( argc != 0 ) exit( 1 );
	if ( ( !physical && ( p = getcwd_logical() ) != NULL ) || ( p = getcwd( NULL, 0 ) ) != NULL ) printf( "%s\n", p );
	else err( 1, "." );
	exit( 0 );
}
static char * getcwd_logical( void ) {
	struct stat lg, phy;
	char *pwd;
	if ( ( pwd = getenv( "PWD" ) ) != NULL && *pwd == '/' ) {
		if ( stat( pwd, &lg ) == -1 || stat( ".", &phy ) == -1 ) return ( NULL );
		if ( lg.st_dev == phy.st_dev && lg.st_ino == phy.st_ino ) return ( pwd );
	}
	errno = ENOENT;
	return ( NULL );
}
