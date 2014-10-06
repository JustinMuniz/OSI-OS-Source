#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifndef NO_UDOM_SUPPORT
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#endif
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int bflag, eflag, lflag, nflag, sflag, tflag, vflag;
static int rval;
static const char *filename;
__dead2;
static void scanfiles( char *argv[], int cooked );
static void cook_cat( FILE * );
static void raw_cat( int );
#ifndef NO_UDOM_SUPPORT
static int udom_open( const char *path, int flags );
#endif
#define	PHYSPAGES_THRESHOLD (32 * 1024)
#define	BUFSIZE_MAX (2 * 1024 * 1024)
#define	BUFSIZE_SMALL (MAXPHYS)
int main( int argc, char *argv[] ) {
	int ch;
	struct flock stdout_lock;
	setlocale( LC_CTYPE, "" );
	while ( ( ch = getopt( argc, argv, "belnstuv" ) ) != -1 )
		switch ( ch ) {
			case 'b':
				bflag = nflag = 1;
				break;
			case 'e':
				eflag = vflag = 1;
				break;
			case 'l':
				lflag = 1;
				break;
			case 'n':
				nflag = 1;
				break;
			case 's':
				sflag = 1;
				break;
			case 't':
				tflag = vflag = 1;
				break;
			case 'u':
				setbuf( stdout, NULL );
				break;
			case 'v':
				vflag = 1;
				break;
			default:
				exit( 1 );
		}
	argv += optind;
	if ( lflag ) {
		stdout_lock.l_len = 0;
		stdout_lock.l_start = 0;
		stdout_lock.l_type = F_WRLCK;
		stdout_lock.l_whence = SEEK_SET;
		if ( fcntl( STDOUT_FILENO, F_SETLKW, &stdout_lock ) == -1 ) err( EXIT_FAILURE, "stdout" );
	}
	if ( bflag || eflag || nflag || sflag || tflag || vflag ) scanfiles( argv, 1 );
	else scanfiles( argv, 0 );
	if ( fclose (stdout) ) err( 1, "stdout" );
	exit( rval );
}
static void scanfiles( char *argv[], int cooked ) {
	int fd, i;
	char *path;
	FILE *fp;
	i = 0;
	while ( ( path = argv[i] ) != NULL || i == 0 ) {
		if ( path == NULL || strcmp( path, "-" ) == 0 ) {
			filename = "stdin";
			fd = STDIN_FILENO;
		} else {
			filename = path;
			fd = open( path, O_RDONLY );
#ifndef NO_UDOM_SUPPORT
			if ( fd < 0 && errno == EOPNOTSUPP ) fd = udom_open( path, O_RDONLY );
#endif
		}
		if ( fd < 0 ) {
			warn( "%s", path );
			rval = 1;
		} else if ( cooked ) {
			if ( fd == STDIN_FILENO ) cook_cat (stdin);
			else {
				fp = fdopen( fd, "r" );
				cook_cat( fp );
				fclose( fp );
			}
		} else {
			raw_cat( fd );
			if ( fd != STDIN_FILENO ) close( fd );
		}
		if ( path == NULL ) break;
		++i;
	}
}
static void cook_cat( FILE *fp ) {
	int ch, gobble, line, prev;
	if ( fp == stdin && feof( stdin ) ) clearerr (stdin);
	line = gobble = 0;
	for ( prev = '\n'; ( ch = getc( fp ) ) != EOF; prev = ch ) {
		if ( prev == '\n' ) {
			if ( sflag ) {
				if ( ch == '\n' ) {
					if ( gobble ) continue;
					gobble = 1;
				} else gobble = 0;
			}
			if ( nflag && ( !bflag || ch != '\n' ) ) {
				(void) fprintf( stdout, "%6d\t", ++line );
				if ( ferror (stdout) ) break;
			}
		}
		if ( ch == '\n' ) {
			if ( eflag && putchar( '$' ) == EOF ) break;
		} else if ( ch == '\t' ) {
			if ( tflag ) {
				if ( putchar( '^' ) == EOF || putchar( 'I' ) == EOF ) break;
				continue;
			}
		} else if ( vflag ) {
			if ( !isascii( ch ) && !isprint( ch ) ) {
				if ( putchar( 'M' ) == EOF || putchar( '-' ) == EOF ) break;
				ch = toascii( ch );
			}
			if ( iscntrl( ch ) ) {
				if ( putchar( '^' ) == EOF || putchar( ch == '\177' ? '?' : ch | 0100 ) == EOF ) break;
				continue;
			}
		}
		if ( putchar( ch ) == EOF ) break;
	}
	if ( ferror( fp ) ) {
		warn( "%s", filename );
		rval = 1;
		clearerr( fp );
	}
	if ( ferror (stdout) ) err( 1, "stdout" );
}
static void raw_cat( int rfd ) {
	int off, wfd;
	ssize_t nr, nw;
	static size_t bsize;
	static char *buf = NULL;
	struct stat sbuf;
	wfd = fileno( stdout );
	if ( buf == NULL ) {
		if ( fstat( wfd, &sbuf ) ) err( 1, "stdout" );
		if ( S_ISREG( sbuf.st_mode ) ) {
			if ( sysconf( _SC_PHYS_PAGES ) > PHYSPAGES_THRESHOLD ) bsize = MIN( BUFSIZE_MAX, MAXPHYS * 8 );
			else bsize = BUFSIZE_SMALL;
		} else bsize = MAX( sbuf.st_blksize, (blksize_t) sysconf( _SC_PAGESIZE ) );
		if ( ( buf = malloc( bsize ) ) == NULL ) err( 1, "malloc() failure of IO buffer" );
	}
	while ( ( nr = read( rfd, buf, bsize ) ) > 0 )
		for ( off = 0; nr; nr -= nw, off += nw )
			if ( ( nw = write( wfd, buf + off, (size_t) nr ) ) < 0 ) err( 1, "stdout" );
	if ( nr < 0 ) {
		warn( "%s", filename );
		rval = 1;
	}
}
#ifndef NO_UDOM_SUPPORT
static int udom_open( const char *path, int flags ) {
	struct sockaddr_un sou;
	int fd;
	unsigned int len;
	bzero( &sou, sizeof( sou ) );
	fd = socket( AF_UNIX, SOCK_STREAM, 0 );
	if ( fd >= 0 ) {
		sou.sun_family = AF_UNIX;
		if ( ( len = strlcpy( sou.sun_path, path, sizeof( sou.sun_path ) ) ) >= sizeof( sou.sun_path ) ) {
			errno = ENAMETOOLONG;
			return ( -1 );
		}
		len = offsetof(struct sockaddr_un, sun_path[len+1]);
		if ( connect( fd, (void *) &sou, len ) < 0 ) {
			close( fd );
			fd = -1;
		}
	}
	if ( fd >= 0 ) {
		switch ( flags & O_ACCMODE ) {
			case O_RDONLY:
				if ( shutdown( fd, SHUT_WR ) == -1 ) warn (NULL);
				break;
			case O_WRONLY:
				if ( shutdown( fd, SHUT_RD ) == -1 ) warn (NULL);
		}
	}
	return ( fd );
}
#endif
