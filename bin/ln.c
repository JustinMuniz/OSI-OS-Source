#if 0
#ifndef lint
static char const copyright[] =
"@(#) Copyright (c) 1987, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif 
#ifndef lint
static char sccsid[] = "@(#)ln.c	8.2 (Berkeley) 3/31/94";
#endif 
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int fflag;
static int Fflag;
static int hflag;
static int iflag;
static int Pflag;
static int sflag;
static int vflag;
static int wflag;
static char linkch;
static int linkit( const char *, const char *, int );
static void usage( void );
int main( int argc, char *argv[] ) {
	struct stat sb;
	char *p, *targetdir;
	int ch, exitval;
	if ( ( p = strrchr( argv[0], '/' ) ) == NULL ) p = argv[0];
	else ++p;
	if ( strcmp( p, "link" ) == 0 ) {
		while ( getopt( argc, argv, "" ) != -1 )
			usage();
		argc -= optind;
		argv += optind;
		if ( argc != 2 ) usage();
		exit( linkit( argv[0], argv[1], 0 ) );
	}
	while ( ( ch = getopt( argc, argv, "FLPfhinsvw" ) ) != -1 )
		switch ( ch ) {
			case 'F':
				Fflag = 1;
				break;
			case 'L':
				Pflag = 0;
				break;
			case 'P':
				Pflag = 1;
				break;
			case 'f':
				fflag = 1;
				iflag = 0;
				wflag = 0;
				break;
			case 'h':
			case 'n':
				hflag = 1;
				break;
			case 'i':
				iflag = 1;
				fflag = 0;
				break;
			case 's':
				sflag = 1;
				break;
			case 'v':
				vflag = 1;
				break;
			case 'w':
				wflag = 1;
				break;
			case '?':
			default:
				usage();
		}
	argv += optind;
	argc -= optind;
	linkch = sflag ? '-' : '=';
	if ( sflag == 0 ) Fflag = 0;
	if ( Fflag == 1 && iflag == 0 ) {
		fflag = 1;
		wflag = 0;
	}
	switch ( argc ) {
		case 0:
			usage();
		case 1:
			exit( linkit( argv[0], ".", 1 ) );
		case 2:
			exit( linkit( argv[0], argv[1], 0 ) );
		default:
			;
	}
	targetdir = argv[argc - 1];
	if ( hflag && lstat( targetdir, &sb ) == 0 && S_ISLNK( sb.st_mode ) ) {
		errno = ENOTDIR;
		err( 1, "%s", targetdir );
	}
	if ( stat( targetdir, &sb ) ) err( 1, "%s", targetdir );
	if ( !S_ISDIR( sb.st_mode ) ) usage();
	for ( exitval = 0; *argv != targetdir; ++argv )
		exitval |= linkit( *argv, targetdir, 1 );
	exit( exitval );
}
static int samedirent( const char *path1, const char *path2 ) {
	const char *file1, *file2;
	char pathbuf[PATH_MAX];
	struct stat sb1, sb2;
	if ( strcmp( path1, path2 ) == 0 ) return 1;
	file1 = strrchr( path1, '/' );
	if ( file1 != NULL ) file1++;
	else file1 = path1;
	file2 = strrchr( path2, '/' );
	if ( file2 != NULL ) file2++;
	else file2 = path2;
	if ( strcmp( file1, file2 ) != 0 ) return 0;
	if ( file1 - path1 >= PATH_MAX || file2 - path2 >= PATH_MAX ) return 0;
	if ( file1 == path1 ) memcpy( pathbuf, ".", 2 );
	else {
		memcpy( pathbuf, path1, file1 - path1 );
		pathbuf[file1 - path1] = '\0';
	}
	if ( stat( pathbuf, &sb1 ) != 0 ) return 0;
	if ( file2 == path2 ) memcpy( pathbuf, ".", 2 );
	else {
		memcpy( pathbuf, path2, file2 - path2 );
		pathbuf[file2 - path2] = '\0';
	}
	if ( stat( pathbuf, &sb2 ) != 0 ) return 0;
	return sb1.st_dev == sb2.st_dev && sb1.st_ino == sb2.st_ino;
}
static int linkit( const char *source, const char *target, int isdir ) {
	struct stat sb;
	const char *p;
	int ch, exists, first;
	char path[PATH_MAX];
	char wbuf[PATH_MAX];
	char bbuf[PATH_MAX];
	if ( !sflag ) {
		if ( ( Pflag ? lstat : stat )( source, &sb ) ) {
			warn( "%s", source );
			return ( 1 );
		}
		if ( S_ISDIR( sb.st_mode ) ) {
			errno = EISDIR;
			warn( "%s", source );
			return ( 1 );
		}
	}
	if ( isdir || ( lstat( target, &sb ) == 0 && S_ISDIR( sb.st_mode ) ) || ( !hflag && stat( target, &sb ) == 0 && S_ISDIR( sb.st_mode ) ) ) {
		if ( strlcpy( bbuf, source, sizeof( bbuf ) ) >= sizeof( bbuf ) || ( p = basename( bbuf ) ) == NULL || snprintf( path, sizeof( path ), "%s/%s", target, p ) >= (ssize_t) sizeof( path ) ) {
			errno = ENAMETOOLONG;
			warn( "%s", source );
			return ( 1 );
		}
		target = path;
	}
	if ( sflag && wflag ) {
		if ( *source == '/' ) {
			if ( stat( source, &sb ) != 0 ) warn( "warning: %s inaccessible", source );
		} else {
			strlcpy( bbuf, target, sizeof( bbuf ) );
			p = dirname( bbuf );
			if ( p != NULL ) {
				(void) snprintf( wbuf, sizeof( wbuf ), "%s/%s", p, source );
				if ( stat( wbuf, &sb ) != 0 ) warn( "warning: %s", source );
			}
		}
	}
	exists = !lstat( target, &sb );
	if ( exists ) {
		if ( !sflag && samedirent( source, target ) ) {
			warnx( "%s and %s are the same directory entry", source, target );
			return ( 1 );
		}
	}
	if ( fflag && exists ) {
		if ( Fflag && S_ISDIR( sb.st_mode ) ) {
			if ( rmdir( target ) ) {
				warn( "%s", target );
				return ( 1 );
			}
		} else if ( unlink( target ) ) {
			warn( "%s", target );
			return ( 1 );
		}
	} else if ( iflag && exists ) {
		fflush (stdout);
		fprintf( stderr, "replace %s? ", target );
		first = ch = getchar();
		while ( ch != '\n' && ch != EOF )
			ch = getchar();
		if ( first != 'y' && first != 'Y' ) {
			fprintf( stderr, "not replaced\n" );
			return ( 1 );
		}
		if ( Fflag && S_ISDIR( sb.st_mode ) ) {
			if ( rmdir( target ) ) {
				warn( "%s", target );
				return ( 1 );
			}
		} else if ( unlink( target ) ) {
			warn( "%s", target );
			return ( 1 );
		}
	}
	if ( sflag ? symlink( source, target ) : linkat( AT_FDCWD, source, AT_FDCWD, target, Pflag ? 0 : AT_SYMLINK_FOLLOW ) ) {
		warn( "%s", target );
		return ( 1 );
	}
	if ( vflag ) (void) printf( "%s %c> %s\n", target, linkch, source );
	return ( 0 );
}
static void usage( void ) {
	(void) fprintf( stderr, "%s\n%s\n%s\n", "usage: ln [-s [-F] | -L | -P] [-f | -i] [-hnv] source_file [target_file]", "       ln [-s [-F] | -L | -P] [-f | -i] [-hnv] source_file ... target_dir", "       link source_file target_file" );
	exit( 1 );
}
