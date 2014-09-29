#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(__FreeBSD_version)
#if __FreeBSD_version < 500000
#define NEED_SLASHTERM
#endif 
#else  
#define NEED_SLASHTERM
#endif 
#define PATHCTL	"kern.module_path"
TAILQ_HEAD(pathhead, pathentry);
struct pathentry {
		char *path;TAILQ_ENTRY(pathentry) next;
};
static int mib[5];
static size_t miblen;
static char *pathctl;
static char *modpath;
static int changed;
static void addpath( struct pathhead *, char *, int, int );
static void rempath( struct pathhead *, char *, int, int );
static void showpath( struct pathhead * );
static char *qstring( struct pathhead * );
static void getmib( void );
static void getpath( void );
static void parsepath( struct pathhead *, char *, int );
static void setpath( struct pathhead * );
static void usage( void );
static void getmib( void ) {
	if ( miblen != 0 ) return;
	miblen = sizeof( mib ) / sizeof( mib[0] );
	if ( sysctlnametomib( pathctl, mib, &miblen ) != 0 ) err( 1, "sysctlnametomib(%s)", pathctl );
}
static void getpath( void ) {
	char *path;
	size_t sz;
	if ( modpath != NULL ) {
		free( modpath );
		modpath = NULL;
	}
	if ( miblen == 0 ) getmib();
	if ( sysctl( mib, miblen, NULL, &sz, NULL, 0 ) == -1 ) err( 1, "getting path: sysctl(%s) - size only", pathctl );
	if ( ( path = malloc( sz + 1 ) ) == NULL ) {
		errno = ENOMEM;
		err( 1, "allocating %lu bytes for the path", (unsigned long) sz + 1 );
	}
	if ( sysctl( mib, miblen, path, &sz, NULL, 0 ) == -1 ) err( 1, "getting path: sysctl(%s)", pathctl );
	modpath = path;
}
static void setpath( struct pathhead *pathq ) {
	char *newpath;
	if ( miblen == 0 ) getmib();
	if ( ( newpath = qstring( pathq ) ) == NULL ) {
		errno = ENOMEM;
		err( 1, "building path string" );
	}
	if ( sysctl( mib, miblen, NULL, NULL, newpath, strlen( newpath ) + 1 ) == -1 ) err( 1, "setting path: sysctl(%s)", pathctl );
	if ( modpath != NULL ) free( modpath );
	modpath = newpath;
}
static void addpath( struct pathhead *pathq, char *path, int force, int insert ) {
	struct pathentry *pe, *pskip;
	char pathbuf[MAXPATHLEN + 1];
	size_t len;
	static unsigned added = 0;
	unsigned i;
	if ( realpath( path, pathbuf ) == NULL ) strlcpy( pathbuf, path, sizeof( pathbuf ) );
	len = strlen( pathbuf );
#ifdef NEED_SLASHTERM
	if ( ( len == 0 ) || ( pathbuf[len - 1] != '/' ) ) {
		if ( len == sizeof( pathbuf ) - 1 ) errx( 1, "path too long: %s", pathbuf );
		pathbuf[len] = '/';
	}
#else  
	if ((len > 0) && (pathbuf[len-1] == '/'))
	pathbuf[--len] = '\0';
#endif 
	TAILQ_FOREACH( pe, pathq, next )
	if ( !strcmp( pe->path, pathbuf ) ) break;
	if ( pe != NULL ) {
		if ( force ) return;
		errx( 1, "already in the module search path: %s", pathbuf );
	}
	if ( ( ( pe = malloc( sizeof( *pe ) ) ) == NULL ) || ( ( pe->path = strdup( pathbuf ) ) == NULL ) ) {
		errno = ENOMEM;
		err( 1, "allocating path component" );
	}
	if ( !insert ) {
		TAILQ_INSERT_TAIL( pathq, pe, next );
	} else {
		for ( i = 0, pskip = TAILQ_FIRST( pathq ); i < added; i++ )
			pskip = TAILQ_NEXT( pskip, next );
		if ( pskip != NULL ) TAILQ_INSERT_BEFORE( pskip, pe, next );
		else TAILQ_INSERT_TAIL( pathq, pe, next );
		added++;
	}
	changed = 1;
}
static void
rempath(struct pathhead *pathq, char *path, int force, int insert __unused)
{	
	char pathbuf[MAXPATHLEN+1];
	struct pathentry *pe;
	size_t len;
	if (realpath(path, pathbuf) == NULL)
	strlcpy(pathbuf, path, sizeof(pathbuf));
	len = strlen(pathbuf);
#ifdef NEED_SLASHTERM
	if ((len == 0) || (pathbuf[len-1] != '/')) {
		if (len == sizeof(pathbuf) - 1)
		errx(1, "path too long: %s", pathbuf);
		pathbuf[len] = '/';
	}
#else  
	if ((len > 0) && (pathbuf[len-1] == '/'))
	pathbuf[--len] = '\0';
#endif 
	TAILQ_FOREACH(pe, pathq, next)
	if (!strcmp(pe->path, pathbuf))
	break;
	if (pe == NULL) {
		if (force)
		return;
		errx(1, "not in module search path: %s", pathbuf);
	}
	TAILQ_REMOVE(pathq, pe, next);
	changed = 1;
}
static void showpath( struct pathhead *pathq ) {
	char *s;
	if ( ( s = qstring( pathq ) ) == NULL ) {
		errno = ENOMEM;
		err( 1, "building path string" );
	}
	printf( "%s\n", s );
	free( s );
}
static void parsepath( struct pathhead *pathq, char *path, int uniq ) {
	char *p;
	struct pathentry *pe;
	while ( ( p = strsep( &path, ";" ) ) != NULL )
		if ( !uniq ) {
			if ( ( ( pe = malloc( sizeof( *pe ) ) ) == NULL ) || ( ( pe->path = strdup( p ) ) == NULL ) ) {
				errno = ENOMEM;
				err( 1, "allocating path element" );
			}
			TAILQ_INSERT_TAIL( pathq, pe, next );
		} else {
			addpath( pathq, p, 1, 0 );
		}
}
static char *
qstring( struct pathhead *pathq ) {
	char *s, *p;
	struct pathentry *pe;
	s = strdup( "" );
	TAILQ_FOREACH( pe, pathq, next )
	{
		asprintf( &p, "%s%s%s", s, pe->path, ( TAILQ_NEXT( pe, next ) != NULL ? ";" : "" ) );
		free( s );
		if ( p == NULL ) return ( NULL );
		s = p;
	}
	return ( s );
}
static void usage( void ) {
	fprintf( stderr, "%s\n%s\n", "usage:\tkldconfig [-dfimnUv] [-S sysctlname] [path ...]", "\tkldconfig -r" );
	exit( 1 );
}
int main( int argc, char *argv[] ) {
	int c;
	int i;
	int fflag;
	int iflag;
	int mflag;
	int nflag;
	int rflag;
	int uniqflag;
	int vflag;
	void (*act)( struct pathhead *, char *, int, int );
	char *origpath;
	struct pathhead pathq;
	fflag = iflag = mflag = nflag = rflag = uniqflag = vflag = 0;
	act = addpath;
	origpath = NULL;
	if ( ( pathctl = strdup( PATHCTL ) ) == NULL ) {
		errno = ENOMEM;
		err( 1, "initializing sysctl name %s", PATHCTL );
	}
	if ( argc == 1 ) mflag = 1;
	while ( ( c = getopt( argc, argv, "dfimnrS:Uv" ) ) != -1 )
		switch ( c ) {
			case 'd':
				if ( iflag || mflag ) usage();
				act = rempath;
				break;
			case 'f':
				fflag = 1;
				break;
			case 'i':
				if ( act != addpath ) usage();
				iflag = 1;
				break;
			case 'm':
				if ( act != addpath ) usage();
				mflag = 1;
				break;
			case 'n':
				nflag = 1;
				break;
			case 'r':
				rflag = 1;
				break;
			case 'S':
				free( pathctl );
				if ( ( pathctl = strdup( optarg ) ) == NULL ) {
					errno = ENOMEM;
					err( 1, "sysctl name %s", optarg );
				}
				break;
			case 'U':
				uniqflag = 1;
				break;
			case 'v':
				vflag++;
				break;
			default:
				usage();
		}
	argc -= optind;
	argv += optind;
	if ( rflag && ( argc > 0 ) ) usage();
	TAILQ_INIT( &pathq );
	getpath();
	if ( ( origpath = strdup( modpath ) ) == NULL ) {
		errno = ENOMEM;
		err( 1, "saving the original search path" );
	}
	if ( ( act != addpath ) || mflag || rflag || uniqflag ) parsepath( &pathq, modpath, uniqflag );
	else if ( modpath[0] != '\0' ) changed = 1;
	for ( i = 0; i < argc; i++ )
		act( &pathq, argv[i], fflag, iflag );
	if ( changed && !nflag ) setpath( &pathq );
	if ( rflag || ( changed && vflag ) ) {
		if ( changed && ( vflag > 1 ) ) printf( "%s -> ", origpath );
		showpath( &pathq );
	}
	return ( 0 );
}
