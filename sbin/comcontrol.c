#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
static void usage( void );
static void usage( void ) {
	fprintf( stderr, "usage: comcontrol <filename> [dtrwait <n>] [drainwait <n>]\n" );
	exit( 1 );
}
int main( int argc, char *argv[] ) {
	int fd;
	int res = 0;
	int print_dtrwait = 1, print_drainwait = 1;
	int dtrwait = -1, drainwait = -1;
	if ( argc < 2 ) usage();
	if ( strcmp( argv[1], "-" ) == 0 ) fd = STDIN_FILENO;
	else {
		fd = open( argv[1], O_RDONLY | O_NONBLOCK, 0 );
		if ( fd < 0 ) {
			warn( "couldn't open file %s", argv[1] );
			return 1;
		}
	}
	if ( argc == 2 ) {
		if ( ioctl( fd, TIOCMGDTRWAIT, &dtrwait ) < 0 ) {
			print_dtrwait = 0;
			if ( errno != ENOTTY ) {
				res = 1;
				warn( "TIOCMGDTRWAIT" );
			}
		}
		if ( ioctl( fd, TIOCGDRAINWAIT, &drainwait ) < 0 ) {
			print_drainwait = 0;
			if ( errno != ENOTTY ) {
				res = 1;
				warn( "TIOCGDRAINWAIT" );
			}
		}
		if ( print_dtrwait ) printf( "dtrwait %d ", dtrwait );
		if ( print_drainwait ) printf( "drainwait %d ", drainwait );
		printf( "\n" );
	} else {
		while ( argv[2] != NULL ) {
			if ( !strcmp( argv[2], "dtrwait" ) ) {
				if ( dtrwait >= 0 ) usage();
				if ( argv[3] == NULL || !isdigit( argv[3][0] ) ) usage();
				dtrwait = atoi( argv[3] );
				argv += 2;
			} else if ( !strcmp( argv[2], "drainwait" ) ) {
				if ( drainwait >= 0 ) usage();
				if ( argv[3] == NULL || !isdigit( argv[3][0] ) ) usage();
				drainwait = atoi( argv[3] );
				argv += 2;
			} else usage();
		}
		if ( dtrwait >= 0 ) {
			if ( ioctl( fd, TIOCMSDTRWAIT, &dtrwait ) < 0 ) {
				res = 1;
				warn( "TIOCMSDTRWAIT" );
			}
		}
		if ( drainwait >= 0 ) {
			if ( ioctl( fd, TIOCSDRAINWAIT, &drainwait ) < 0 ) {
				res = 1;
				warn( "TIOCSDRAINWAIT" );
			}
		}
	}
	close( fd );
	return res;
}
